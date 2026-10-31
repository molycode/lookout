#include "query_pump.hpp"
#include "endpoint.hpp"
#include "loggers.hpp"
#include "query/master_endpoint.hpp"
#include "query/protocol_definition.hpp"
#include <tge/assert.hpp>
#include <tge/net/ipv4_endpoint.hpp>
#include <tge/net/parse_ipv4.hpp>
#include <tge/net/stream_socket.hpp>
#include <tge/net/udp_socket.hpp>
#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace Lkt::Net
{
namespace
{
constexpr size_t MaxDatagramSize{ 65536 };
constexpr size_t MaxDatagramsPerWake{ 256 };
constexpr size_t MaxStreamReadsPerWake{ 16 };
constexpr size_t StreamReadSize{ 16u << 10 };
// The busiest master's list is a few tens of KiB; a stream far beyond that is broken or hostile.
constexpr size_t MaxStreamBytes{ 4u << 20 };
constexpr std::chrono::milliseconds LookupPollInterval{ 20 };
// A master of a thousand servers and dozens of status replies can land at once; the default buffer drops some.
constexpr uint32_t ReceiveBufferSize{ 1u << 20 };

//////////////////////////////////////////////////////////////////////////
std::optional<Clock::time_point> Earliest(std::optional<Clock::time_point> first, std::optional<Clock::time_point> second)
{
	std::optional<Clock::time_point> earliest{ first.has_value() ? first : second };

	if (first.has_value() && second.has_value())
	{
		earliest = std::min(first.value(), second.value());
	}

	return earliest;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::error_code> OpenDatagramSocket(Tge::Net::CUdpSocket& socket)
{
	std::expected<void, std::error_code> const opened{ socket.Initialize(Tge::Net::SIpv4Endpoint{}) };

	if (opened.has_value())
	{
		std::expected<void, std::error_code> const enlarged{ socket.SetReceiveBufferSize(ReceiveBufferSize) };

		if (!enlarged.has_value())
		{
			gLog.Warning("Cannot enlarge the receive buffer, replies may be dropped under load: {}", enlarged.error().message());
		}
	}
	else
	{
		gLog.Error("Cannot create the UDP socket: {}", opened.error().message());
	}

	return opened;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::error_code> OpenStreamSocket(Tge::Net::CStreamSocket& socket)
{
	std::expected<void, std::error_code> const opened{ socket.Initialize() };

	if (!opened.has_value())
	{
		gLog.Error("Cannot create a TCP socket: {}", opened.error().message());
	}

	return opened;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::error_code> SendWhole(Tge::Net::CStreamSocket const& socket, std::span<std::byte const> bytes)
{
	return socket.Send(bytes).and_then([&bytes](size_t numSent)
	{
		return (numSent == bytes.size()) ? std::expected<void, std::error_code>{} : std::unexpected{ std::make_error_code(std::errc::operation_would_block) };
	});
}

//////////////////////////////////////////////////////////////////////////
uint64_t ToConversationKey(Query::EGame game, Query::SServerAddress const& address)
{
	return (static_cast<uint64_t>(game) << 48) | Query::ToKey(address);
}

//////////////////////////////////////////////////////////////////////////
uint64_t ToConversationKey(SMasterId const& master)
{
	return (static_cast<uint64_t>(master.game) << 48) | static_cast<uint64_t>(master.index);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> StartConversation(Script::CProtocolScript& script, std::map<std::string, std::string> const& options,
	SConversationRecord& record)
{
	std::expected<Script::SScriptAction, std::string> started{ script.Start(record.conversation, options) };
	std::expected<void, std::string> result{};

	if (started.has_value())
	{
		record.send = std::move(started->send);
	}
	else
	{
		result = std::unexpected{ std::move(started.error()) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Nothing the script made of the datagram: it may answer an earlier step, or wait for the rest of a reply.
bool IsEmpty(Script::SScriptAction const& action)
{
	return action.send.empty() && action.servers.empty() && !action.reply.has_value() && !action.reason.has_value() && !action.quiet.has_value()
		&& !action.isDone;
}

//////////////////////////////////////////////////////////////////////////
void RecordScriptFailure(SRefreshStats& stats, std::string_view failure)
{
	if (stats.numScriptFailures == 0)
	{
		stats.firstScriptFailure = failure;
	}

	++stats.numScriptFailures;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CQueryPump::Initialize(SQueryCatalog catalog, std::function<void()> onEventsReady)
{
	TGE_ASSERT(catalog.games.size() < static_cast<size_t>(Query::NoGame), "More games than EGame can number");
	TGE_ASSERT(std::ranges::all_of(catalog.games, [&catalog](Query::SGameDefinition const& game) { return static_cast<size_t>(game.protocol) < catalog.protocols.size(); }),
		"A game names a protocol the catalog does not have");

	m_onEventsReady = std::move(onEventsReady);
	m_catalog = catalog;
	m_buffer.resize(MaxDatagramSize);
	m_refreshes.assign(catalog.games.size(), SRefreshState{});

	std::span<Query::SProtocolDefinition const> const protocols{ catalog.protocols };
	bool areScriptsReady{ true };

	// Each script's state is created here and used only on the LookoutNet thread, which starts after this.
	m_scripts = std::vector<Script::CProtocolScript>(protocols.size());

	for (size_t index{ 0 }; index < protocols.size(); ++index)
	{
		std::expected<void, std::string> const loaded{ m_scripts[index].Initialize(protocols[index].name, protocols[index].source) };

		if (!loaded.has_value())
		{
			gLog.Error("The protocol script '{}' cannot be loaded: {}", protocols[index].name, loaded.error());
			areScriptsReady = false;
		}
	}

	bool const isReady{ areScriptsReady && OpenDatagramSocket(m_socket).has_value() && m_loop.Initialize("LookoutNet") };

	if (isReady)
	{
		std::expected<void, std::error_code> const disabled{ m_socket.DisableConnectionResetReports() };

		if (!disabled.has_value())
		{
			gLog.Warning("Cannot turn off connection reset reports, unreachable servers may count as receive errors: {}", disabled.error().message());
		}

		m_loop.Post([this]()
		{
			std::optional<Tge::Threading::SWatchId> const watch{ m_loop.Watch(m_socket.GetWaitable(), [this]()
			{
				ReceiveDatagrams();
				Update(Clock::now());
			}) };

			if (!watch.has_value())
			{
				gLog.Error("The query engine cannot receive replies; every refresh will time out");
			}
		});
	}

	return isReady;
}

//////////////////////////////////////////////////////////////////////////
// The loop stops first, so no callback can touch the lookups or the socket while they go.
void CQueryPump::Terminate()
{
	m_loop.Terminate();
	AbandonLookups();
	m_socket.Terminate();

	for (auto& [key, conversation] : m_masterConversations)
	{
		conversation.datagramSocket.Terminate();
		conversation.streamSocket.Terminate();
	}

	m_masterConversations.clear();
	m_serverConversations.clear();

	for (Script::CProtocolScript& script : m_scripts)
	{
		script.Terminate();
	}

	m_scripts.clear();
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::Refresh(Query::EGame game, std::vector<Query::SServerAddress> favourites, uint32_t refreshId)
{
	m_loop.Post([this, game, favourites = std::move(favourites), refreshId]()
	{
		Clock::time_point const now{ Clock::now() };

		StartRefresh(game, ERefreshKind::Full, favourites, refreshId, now);
		Update(now);
	});
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::ProbeMasters(Query::EGame game, uint32_t refreshId)
{
	m_loop.Post([this, game, refreshId]()
	{
		Clock::time_point const now{ Clock::now() };

		StartRefresh(game, ERefreshKind::Probe, {}, refreshId, now);
		Update(now);
	});
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::RefreshServer(Query::EGame game, Query::SServerAddress const& address, uint32_t refreshId)
{
	m_loop.Post([this, game, address, refreshId]()
	{
		Clock::time_point const now{ Clock::now() };

		StartServerRefresh(game, address, refreshId, now);
		Update(now);
	});
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::Cancel(Query::EGame game)
{
	m_loop.Post([this, game]()
	{
		CancelRefresh(game);
		Update(Clock::now());
	});
}

//////////////////////////////////////////////////////////////////////////
// Cleared before draining, and by an exchange: a plain store could be reordered past the drain, and an event the
// pump queued meanwhile would then be announced to nobody.
void CQueryPump::TakeEvents(std::vector<SQueryEvent>& events)
{
	m_isNotified.exchange(false, std::memory_order_acq_rel);

	SQueryEvent event{};

	while (m_events.Dequeue(event))
	{
		events.emplace_back(std::move(event));
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::Update(Clock::time_point now)
{
	CollectLookups(now);
	UpdateMasters(now);
	SendDueRequests(now);
	EndRequests(now);
	FinishRefreshes();
	NotifyIfNeeded();
	ArmTimer(now);
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::ArmTimer(Clock::time_point now)
{
	std::optional<Clock::time_point> deadline{ Earliest(m_scheduler.GetNextDeadline(), m_masters.GetNextDeadline()) };

	if (!m_lookups.empty())
	{
		deadline = Earliest(deadline, now + LookupPollInterval);
	}

	m_loop.CancelTimer(m_timer);

	if (deadline.has_value())
	{
		m_timer = m_loop.ScheduleAt(deadline.value(), [this]() { Update(Clock::now()); });
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::StartRefresh(Query::EGame game, ERefreshKind kind, std::span<Query::SServerAddress const> favourites, uint32_t refreshId,
	Clock::time_point now)
{
	SRefreshState& refresh{ GetRefresh(game) };
	uint32_t const generation{ refreshId };

	TGE_ASSERT(generation > refresh.generation, "A refresh id that is not newer than the game's last one");

	m_scheduler.Cancel(game);
	CloseConversations(game);
	refresh = SRefreshState{};
	refresh.isActive = true;
	refresh.kind = kind;
	refresh.generation = generation;
	refresh.startedAt = now;

	m_masters.Begin(game, generation, GetGame(game).masters, GetScript(game).GetMasterTransport(), now);
	ResolveMasters(game, generation, now);
	ListServers(game, favourites);
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::StartServerRefresh(Query::EGame game, Query::SServerAddress const& address, uint32_t refreshId, Clock::time_point now)
{
	SRefreshState& refresh{ GetRefresh(game) };

	TGE_ASSERT(!refresh.isActive || refresh.kind == ERefreshKind::Full, "A server refresh would join a probe, which asks no server");

	if (!refresh.isActive)
	{
		uint32_t const generation{ refreshId };

		TGE_ASSERT(generation > refresh.generation, "A refresh id that is not newer than the game's last one");

		refresh = SRefreshState{};
		refresh.isActive = true;
		refresh.generation = generation;
		refresh.startedAt = now;
	}

	if (refresh.knownServers.insert(Query::ToKey(address)).second)
	{
		++refresh.stats.numListed;
		Emit(SServersListed{ game, { address } });
	}

	if (!m_scheduler.IsQueued(game, address))
	{
		m_scheduler.Add(SServerRequest{ game, address });
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::CancelRefresh(Query::EGame game)
{
	SRefreshState& refresh{ GetRefresh(game) };

	m_scheduler.Cancel(game);
	m_masters.Cancel(game);
	CloseConversations(game);

	if (refresh.isActive)
	{
		refresh.isActive = false;
		Emit(SRefreshFinished{ game });
	}
}

//////////////////////////////////////////////////////////////////////////
// A literal address needs no lookup, nor the glibc thread a lookup starts.
void CQueryPump::ResolveMasters(Query::EGame game, uint32_t generation, Clock::time_point now)
{
	std::span<Query::SMasterEndpoint const> const masters{ GetGame(game).masters };

	for (size_t index{ 0 }; index < masters.size(); ++index)
	{
		std::optional<uint32_t> const literal{ Tge::Net::ParseIpv4(masters[index].host) };

		if (literal.has_value())
		{
			m_masters.OnResolved(game, generation, index, *literal, now);
		}
		else
		{
			Resolve(game, generation, index, now);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::Resolve(Query::EGame game, uint32_t generation, size_t index, Clock::time_point now)
{
	auto lookup{ std::make_unique<SDnsLookup>(game, generation, index) };
	std::expected<void, std::error_code> const started{ lookup->lookup.Start(GetGame(game).masters[index].host) };

	if (started.has_value())
	{
		m_lookups.emplace_back(std::move(lookup));
	}
	else
	{
		m_masters.OnResolved(game, generation, index, std::unexpected{ started.error().message() }, now);
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::CollectLookups(Clock::time_point now)
{
	std::erase_if(m_lookups, [this, now](std::unique_ptr<SDnsLookup> const& lookup)
	{
		std::optional<std::expected<uint32_t, std::error_code>> const result{ lookup->lookup.Poll() };

		if (result.has_value())
		{
			m_masters.OnResolved(lookup->game, lookup->generation, lookup->masterIndex,
				result->transform_error([](std::error_code const& error) { return error.message(); }), now);
		}

		return result.has_value();
	});
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::AbandonLookups()
{
	for (std::unique_ptr<SDnsLookup> const& lookup : m_lookups)
	{
		lookup->lookup.Terminate();
	}

	m_lookups.clear();
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::UpdateMasters(Clock::time_point now)
{
	m_masters.Update(now, m_masterQueries, m_masterOutcomes);

	for (SMasterQuery const& query : m_masterQueries)
	{
		auto const it{ m_masterConversations.find(ToConversationKey(query.master)) };
		std::expected<void, std::string> const asked{ (it != m_masterConversations.end()) ? SendToMaster(it->second) : OpenMaster(query) };

		if (!asked.has_value())
		{
			m_masters.Fail(query.master, asked.error(), m_masterOutcomes);
		}
	}

	for (SMasterOutcome const& outcome : m_masterOutcomes)
	{
		auto const conversation{ m_masterConversations.find(ToConversationKey(outcome.master)) };
		size_t const numServers{ (conversation != m_masterConversations.end()) ? conversation->second.listed.size() : 0 };

		CloseMaster(ToConversationKey(outcome.master));

		if (outcome.failure.empty())
		{
			m_masterWarnings.RecordAnswer(outcome.master);
			Emit(SMasterAnswered{ outcome.master.game, outcome.master.index, static_cast<uint32_t>(numServers) });
		}
		else
		{
			if (m_masterWarnings.RecordFailure(outcome.master, outcome.failure))
			{
				gLog.Warning("{}: master {} {}", GetGame(outcome.master.game).name, outcome.host, outcome.failure);
			}

			Emit(SMasterFailed{ outcome.master.game, outcome.master.index, std::string{ outcome.host }, outcome.failure });
		}
	}

	m_masterQueries.clear();
	m_masterOutcomes.clear();
}

//////////////////////////////////////////////////////////////////////////
// A failure leaves whatever was opened for the outcome to close.
std::expected<void, std::string> CQueryPump::OpenMaster(SMasterQuery const& query)
{
	SMasterConversation& conversation{ m_masterConversations.try_emplace(ToConversationKey(query.master)).first->second };
	std::expected<void, std::string> result{};

	conversation.record.game = query.master.game;
	conversation.record.conversation.kind = Script::EConversationKind::Master;
	conversation.address = query.address;
	conversation.isStream = GetScript(query.master.game).GetMasterTransport() == Script::EMasterTransport::Tcp;

	std::expected<void, std::error_code> const opened{ conversation.isStream ? OpenStreamSocket(conversation.streamSocket)
		: OpenDatagramSocket(conversation.datagramSocket) };
	std::expected<void, std::error_code> const connected{ !opened.has_value() ? opened
		: conversation.isStream ? conversation.streamSocket.Connect(ToEndpoint(query.address)) : conversation.datagramSocket.Connect(ToEndpoint(query.address)) };

	if (!opened.has_value())
	{
		result = std::unexpected{ std::string{ "cannot be asked: no socket can be created for it" } };
	}
	else if (!connected.has_value())
	{
		result = std::unexpected{ (connected.error() == std::errc::connection_refused) ? std::string{ "refused the query" }
			: std::format("cannot be asked: {}", connected.error().message()) };
	}
	else
	{
		Tge::Threading::SWaitable const waitable{ conversation.isStream ? conversation.streamSocket.GetWaitable() : conversation.datagramSocket.GetWaitable() };

		conversation.watch = m_loop.Watch(waitable, [this, master = query.master]()
		{
			ReceiveFromMaster(master);
			Update(Clock::now());
		});

		result = conversation.watch.has_value() ? StartConversation(GetScript(query.master.game), GetGame(query.master.game).protocolOptions, conversation.record)
			: std::unexpected{ std::string{ "its socket cannot be watched" } };
		result = result.transform_error([](std::string const& failure) { return std::format("cannot be asked: {}", failure); });
	}

	if (result.has_value())
	{
		result = SendToMaster(conversation);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> CQueryPump::SendToMaster(SMasterConversation const& conversation) const
{
	std::expected<void, std::string> result{};

	for (std::vector<std::byte> const& bytes : conversation.record.send)
	{
		std::expected<void, std::error_code> const sent{ conversation.isStream ? SendWhole(conversation.streamSocket, bytes)
			: conversation.datagramSocket.Send(ToEndpoint(conversation.address), bytes) };

		if (result.has_value() && !sent.has_value())
		{
			result = std::unexpected{ std::format("cannot be asked: {}", sent.error().message()) };
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Stops at a master that ended meanwhile; its outcome, handled by the update after this, closes the socket. A stream's
// end is handed to the script as empty data, and a list it does not then call done was cut short.
void CQueryPump::ReceiveFromMaster(SMasterId const& master)
{
	uint64_t const key{ ToConversationKey(master) };
	bool isReading{ true };
	size_t numReads{ 0 };

	while (isReading)
	{
		auto const it{ m_masterConversations.find(key) };

		isReading = it != m_masterConversations.end() && m_masters.IsAsking(master);

		if (isReading)
		{
			SMasterConversation& conversation{ it->second };
			Tge::Net::SIpv4Endpoint source{};
			std::expected<size_t, std::error_code> const received{ conversation.isStream
				? conversation.streamSocket.Receive(std::span<std::byte>{ m_buffer }.first(StreamReadSize))
				: conversation.datagramSocket.Receive(m_buffer, source) };
			Clock::time_point const now{ Clock::now() };

			if (!received.has_value())
			{
				isReading = false;

				if (received.error() != std::errc::operation_would_block)
				{
					m_masters.Fail(master, (received.error() == std::errc::connection_refused) ? std::string{ "refused the query" }
						: std::format("cannot be read: {}", received.error().message()), m_masterOutcomes);
				}
			}
			else if (conversation.isStream && *received == 0)
			{
				isReading = false;
				ReadMasterData(master, conversation, {}, now);

				if (m_masters.IsAsking(master))
				{
					m_masters.Fail(master, "closed the connection before its list was complete", m_masterOutcomes);
				}
			}
			else if (conversation.isStream && conversation.numStreamBytes + *received > MaxStreamBytes)
			{
				isReading = false;
				m_masters.Fail(master, "sent more than one master may", m_masterOutcomes);
			}
			else
			{
				conversation.numStreamBytes += conversation.isStream ? *received : 0;
				ReadMasterData(master, conversation, std::span<std::byte const>{ m_buffer.data(), *received }, now);
				++numReads;
				isReading = numReads < (conversation.isStream ? MaxStreamReadsPerWake : MaxDatagramsPerWake);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// A failing script ends the master: what it would make of the rest cannot be trusted.
void CQueryPump::ReadMasterData(SMasterId const& master, SMasterConversation& conversation, std::span<std::byte const> data, Clock::time_point now)
{
	SRefreshStats& stats{ GetRefresh(master.game).stats };

	m_masters.OnDatagram(master, now);

	std::expected<Script::SScriptAction, std::string> action{ GetScript(master.game).Receive(conversation.record.conversation, data) };

	if (!action.has_value())
	{
		m_masters.Fail(master, std::format("cannot be read: {}", action.error()), m_masterOutcomes);
	}
	else
	{
		size_t const numAdmitted{ m_masters.AdmitEntries(master, action->servers.size()) };

		// A stream keeps its order, so whatever arrives answers the step, even a frame still incomplete.
		if (conversation.isStream || !IsEmpty(*action))
		{
			m_masters.MarkStepAnswered(master);
		}

		if (action->reason.has_value())
		{
			stats.firstBadMasterDatagramError = (stats.numBadMasterDatagrams == 0) ? *action->reason : stats.firstBadMasterDatagramError;
			++stats.numBadMasterDatagrams;
		}

		for (Query::SServerAddress const& address : std::span<Query::SServerAddress const>{ action->servers }.first(numAdmitted))
		{
			if (Query::MayList(conversation.address, address))
			{
				m_listed.emplace_back(address);
			}
			else
			{
				stats.firstUnqueryable = (stats.numUnqueryable == 0) ? address : stats.firstUnqueryable;
				++stats.numUnqueryable;
			}
		}

		// A master may list a server again on a later page.
		for (Query::SServerAddress const& address : m_listed)
		{
			conversation.listed.insert(Query::ToKey(address));
		}

		if (GetRefresh(master.game).kind == ERefreshKind::Full)
		{
			ListServers(master.game, m_listed);
		}

		m_listed.clear();

		if (numAdmitted < action->servers.size())
		{
			m_masters.Fail(master, "listed more servers than one master may; the rest were left out", m_masterOutcomes);
		}
		else if (action->isDone)
		{
			m_masters.Finish(master, m_masterOutcomes);
		}
		else if (!action->send.empty())
		{
			conversation.record.send = std::move(action->send);
			m_masters.BeginStep(master, now);

			if (std::expected<void, std::string> const sent{ SendToMaster(conversation) }; !sent.has_value())
			{
				m_masters.Fail(master, sent.error(), m_masterOutcomes);
			}
		}
		else if (action->quiet.has_value())
		{
			m_masters.SetQuiet(master, *action->quiet);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Unwatched before it closes: a stale watch would keep a descriptor number the next socket may be given.
void CQueryPump::CloseMaster(uint64_t key)
{
	auto const it{ m_masterConversations.find(key) };

	if (it != m_masterConversations.end())
	{
		if (it->second.watch.has_value())
		{
			m_loop.Unwatch(*it->second.watch);
		}

		it->second.datagramSocket.Terminate();
		it->second.streamSocket.Terminate();
		GetScript(it->second.record.game).End(it->second.record.conversation);
		m_masterConversations.erase(it);
	}
}

//////////////////////////////////////////////////////////////////////////
// A server whose script cannot start is out of flight at once, so it is never also reported as silent.
void CQueryPump::SendDueRequests(Clock::time_point now)
{
	m_scheduler.TakeDue(now, m_requests);

	for (SServerRequest const& request : m_requests)
	{
		uint64_t const key{ ToConversationKey(request.game, request.address) };

		TGE_ASSERT(m_serverConversations.contains(key) == (request.numAttempts > 1), "A server's conversation must span exactly its attempts");

		auto it{ m_serverConversations.find(key) };
		std::expected<void, std::string> started{};

		if (it == m_serverConversations.end())
		{
			it = m_serverConversations.try_emplace(key, SConversationRecord{ request.game, Script::SConversation{ Script::EConversationKind::Server, 0 }, {} }).first;
			started = StartConversation(GetScript(request.game), GetGame(request.game).protocolOptions, it->second);
		}

		if (started.has_value())
		{
			SendToServer(request, it->second);
		}
		else
		{
			m_serverConversations.erase(it);
			m_scheduler.Remove(request);
			RecordScriptFailure(GetRefresh(request.game).stats, started.error());
			Emit(SServerFailed{ request.game, request.address, EServerFailure::BadReply });
		}
	}

	m_requests.clear();
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::SendToServer(SServerRequest const& request, SConversationRecord const& record)
{
	SRefreshStats& stats{ GetRefresh(request.game).stats };
	bool hasFailed{ false };

	for (std::vector<std::byte> const& datagram : record.send)
	{
		std::expected<void, std::error_code> const sent{ m_socket.Send(ToEndpoint(request.address), datagram) };

		if (!sent.has_value() && !hasFailed)
		{
			stats.firstSendFailure = (stats.numSendFailures == 0) ? request.address : stats.firstSendFailure;
			stats.firstSendError = (stats.numSendFailures == 0) ? sent.error() : stats.firstSendError;
			++stats.numSendFailures;
			hasFailed = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// A server that said something is given its finish; one that never did did not answer.
void CQueryPump::EndRequests(Clock::time_point now)
{
	m_scheduler.TakeEnded(now, m_endedRequests);

	for (SEndedRequest const& ended : m_endedRequests)
	{
		SRefreshStats& stats{ GetRefresh(ended.request.game).stats };

		if (ended.hasAnswered)
		{
			FinishServer(ended.request, ended.roundTrip);
		}
		else
		{
			CloseServer(ended.request);
			++stats.numNoAnswer;
			Emit(SServerFailed{ ended.request.game, ended.request.address, EServerFailure::NoAnswer });
		}
	}

	m_endedRequests.clear();
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::FinishRefreshes()
{
	for (size_t index{ 0 }; index < m_refreshes.size(); ++index)
	{
		Query::EGame const game{ static_cast<Query::EGame>(index) };
		SRefreshState& refresh{ m_refreshes[index] };

		if (refresh.isActive && !m_masters.HasWork(game) && !m_scheduler.HasWork(game))
		{
			TGE_ASSERT(std::ranges::none_of(m_serverConversations, [game](auto const& entry) { return entry.second.game == game; })
				&& std::ranges::none_of(m_masterConversations, [game](auto const& entry) { return entry.second.record.game == game; }),
				"A finished refresh left a conversation open");

			refresh.isActive = false;

			if (refresh.kind == ERefreshKind::Full)
			{
				ReportRefresh(game);
			}

			Emit(SRefreshFinished{ game });
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::ReceiveDatagrams()
{
	bool isDraining{ true };
	size_t numReceived{ 0 };

	while (isDraining && numReceived < MaxDatagramsPerWake)
	{
		Tge::Net::SIpv4Endpoint source{};
		std::expected<size_t, std::error_code> const received{ m_socket.Receive(m_buffer, source) };

		if (received.has_value())
		{
			Clock::time_point const now{ Clock::now() };
			std::span<std::byte const> const datagram{ m_buffer.data(), received.value() };
			std::optional<SAnsweredRequest> const answered{ m_scheduler.OnDatagram(ToServerAddress(source), datagram.size(), now) };

			if (answered.has_value())
			{
				ReadStatusDatagram(answered.value(), datagram, now);
			}

			++numReceived;
		}
		else
		{
			isDraining = false;

			if (received.error() != std::errc::operation_would_block)
			{
				CountReceiveError(received.error());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Not tied to one server, so every running refresh reports it.
void CQueryPump::CountReceiveError(std::error_code error)
{
	for (SRefreshState& refresh : m_refreshes)
	{
		if (refresh.isActive)
		{
			refresh.stats.firstReceiveError = (refresh.stats.numReceiveErrors == 0) ? error : refresh.stats.firstReceiveError;
			++refresh.stats.numReceiveErrors;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// A datagram past the caps never reaches the script: the conversation ends with what it has.
void CQueryPump::ReadStatusDatagram(SAnsweredRequest const& answered, std::span<std::byte const> datagram, Clock::time_point now)
{
	SServerRequest const& request{ answered.request };
	auto const it{ m_serverConversations.find(ToConversationKey(request.game, request.address)) };

	TGE_ASSERT(it != m_serverConversations.end(), "A datagram for a server without a conversation");

	if (answered.isOverCap)
	{
		SRefreshStats& stats{ GetRefresh(request.game).stats };

		stats.firstOverCap = (stats.numOverCap == 0) ? request.address : stats.firstOverCap;
		++stats.numOverCap;
		FinishServer(request, answered.roundTrip);
	}
	else
	{
		std::expected<Script::SScriptAction, std::string> action{ GetScript(request.game).Receive(it->second.conversation, datagram) };

		if (!action.has_value() || action->reply.has_value() || action->reason.has_value())
		{
			EndServer(request, answered.roundTrip, std::move(action));
		}
		else
		{
			if (!IsEmpty(*action))
			{
				m_scheduler.MarkStepAnswered(request);
			}

			if (!action->send.empty())
			{
				it->second.send = std::move(action->send);
				m_scheduler.BeginStep(request, now);
				SendToServer(request, it->second);
			}
			else if (action->quiet.has_value())
			{
				m_scheduler.SetQuiet(request, *action->quiet);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::FinishServer(SServerRequest const& request, Clock::duration roundTrip)
{
	auto const it{ m_serverConversations.find(ToConversationKey(request.game, request.address)) };

	TGE_ASSERT(it != m_serverConversations.end(), "Finishing a server without a conversation");

	EndServer(request, roundTrip, GetScript(request.game).Finish(it->second.conversation));
}

//////////////////////////////////////////////////////////////////////////
// A conversation that ends with neither reply nor reason left its reply unfinished.
void CQueryPump::EndServer(SServerRequest const& request, Clock::duration roundTrip, std::expected<Script::SScriptAction, std::string> action)
{
	SRefreshStats& stats{ GetRefresh(request.game).stats };

	CloseServer(request);
	m_scheduler.Remove(request);

	if (action.has_value() && action->reply.has_value())
	{
		uint32_t const pingMs{ static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(roundTrip).count()) };

		++stats.numAnswered;
		stats.numMalformedPlayerLines += action->reply->numMalformedPlayerLines;
		Emit(SServerAnswered{ request.game, request.address, pingMs, std::move(*action->reply) });
	}
	else
	{
		Query::EParseError const error{ action.has_value() ? action->reason.value_or(Query::EParseError::Truncated) : Query::EParseError::ScriptFailed };

		if (!action.has_value())
		{
			RecordScriptFailure(stats, action.error());
		}

		stats.firstBadReply = (stats.numBadReplies == 0) ? request.address : stats.firstBadReply;
		stats.firstBadReplyError = (stats.numBadReplies == 0) ? error : stats.firstBadReplyError;
		++stats.numBadReplies;
		Emit(SServerFailed{ request.game, request.address, EServerFailure::BadReply });
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::CloseServer(SServerRequest const& request)
{
	auto const it{ m_serverConversations.find(ToConversationKey(request.game, request.address)) };

	if (it != m_serverConversations.end())
	{
		GetScript(request.game).End(it->second.conversation);
		m_serverConversations.erase(it);
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::ListServers(Query::EGame game, std::span<Query::SServerAddress const> servers)
{
	SRefreshState& refresh{ GetRefresh(game) };
	SServersListed listed{ game, {} };

	for (Query::SServerAddress const& address : servers)
	{
		if (refresh.knownServers.insert(Query::ToKey(address)).second)
		{
			m_scheduler.Add(SServerRequest{ game, address });
			listed.servers.emplace_back(address);
		}
	}

	if (!listed.servers.empty())
	{
		refresh.stats.numListed += listed.servers.size();
		Emit(std::move(listed));
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::CloseConversations(Query::EGame game)
{
	for (auto it{ m_serverConversations.begin() }; it != m_serverConversations.end();)
	{
		if (it->second.game == game)
		{
			GetScript(game).End(it->second.conversation);
			it = m_serverConversations.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (auto it{ m_masterConversations.begin() }; it != m_masterConversations.end();)
	{
		uint64_t const key{ it->first };
		bool const isGame{ it->second.record.game == game };

		++it;

		if (isGame)
		{
			CloseMaster(key);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::ReportRefresh(Query::EGame game) const
{
	SRefreshState const& refresh{ GetRefresh(game) };
	SRefreshStats const& stats{ refresh.stats };
	std::string_view const name{ GetGame(game).name };
	double const seconds{ std::chrono::duration<double>{ Clock::now() - refresh.startedAt }.count() };

	gLog.Info("{}: {} of {} servers answered in {:.1f} s", name, stats.numAnswered, stats.numListed, seconds);

	if (stats.numNoAnswer > 0 && stats.numNoAnswer == stats.numListed)
	{
		gLog.Warning("{}: no server answered ({} listed)", name, stats.numListed);
	}

	if (stats.numBadReplies == 1)
	{
		gLog.Warning("{}: the status reply from {} was unreadable ({})", name, Query::FormatAddress(stats.firstBadReply), Query::ToString(stats.firstBadReplyError));
	}
	else if (stats.numBadReplies > 1)
	{
		gLog.Warning("{}: {} status replies were unreadable, first from {} ({})", name, stats.numBadReplies, Query::FormatAddress(stats.firstBadReply), Query::ToString(stats.firstBadReplyError));
	}

	if (stats.numScriptFailures == 1)
	{
		gLog.Warning("{}: the protocol script failed: {}", name, stats.firstScriptFailure);
	}
	else if (stats.numScriptFailures > 1)
	{
		gLog.Warning("{}: the protocol script failed {} times, first: {}", name, stats.numScriptFailures, stats.firstScriptFailure);
	}

	if (stats.numMalformedPlayerLines > 0)
	{
		gLog.Warning("{}: {} player {} unreadable", name, stats.numMalformedPlayerLines, (stats.numMalformedPlayerLines == 1) ? "line was" : "lines were");
	}

	if (stats.numUnqueryable == 1)
	{
		gLog.Warning("{}: a master listed {}, which no public server can have; it was not asked", name, Query::FormatAddress(stats.firstUnqueryable));
	}
	else if (stats.numUnqueryable > 1)
	{
		gLog.Warning("{}: masters listed {} addresses no public server can have, first {}; they were not asked", name, stats.numUnqueryable, Query::FormatAddress(stats.firstUnqueryable));
	}

	if (stats.numBadMasterDatagrams > 0)
	{
		gLog.Warning("{}: {} master {} unreadable ({}); {} complete entries were kept", name, stats.numBadMasterDatagrams,
			(stats.numBadMasterDatagrams == 1) ? "datagram was" : "datagrams were", Query::ToString(stats.firstBadMasterDatagramError),
			(stats.numBadMasterDatagrams == 1) ? "its" : "their");
	}

	if (stats.numOverCap == 1)
	{
		gLog.Warning("{}: {} sent more than one server may; it was cut short", name, Query::FormatAddress(stats.firstOverCap));
	}
	else if (stats.numOverCap > 1)
	{
		gLog.Warning("{}: {} servers sent more than one server may; each was cut short, first {}", name, stats.numOverCap, Query::FormatAddress(stats.firstOverCap));
	}

	if (stats.numSendFailures == 1)
	{
		gLog.Warning("{}: the status request to {} could not be sent: {}", name, Query::FormatAddress(stats.firstSendFailure), stats.firstSendError.message());
	}
	else if (stats.numSendFailures > 1)
	{
		gLog.Warning("{}: {} status requests could not be sent, first to {}: {}", name, stats.numSendFailures, Query::FormatAddress(stats.firstSendFailure), stats.firstSendError.message());
	}

	if (stats.numReceiveErrors > 0)
	{
		gLog.Warning("{}: receiving failed {} {}: {}", name, stats.numReceiveErrors, (stats.numReceiveErrors == 1) ? "time" : "times", stats.firstReceiveError.message());
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::Emit(SQueryEvent event)
{
	std::visit([this](auto& typed)
	{
		typed.refreshId = GetRefresh(typed.game).generation;
	}, event);

	m_events.Enqueue(std::move(event));
	m_hasNewEvents = true;
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::NotifyIfNeeded()
{
	if (m_hasNewEvents)
	{
		m_hasNewEvents = false;

		if (!m_isNotified.exchange(true, std::memory_order_acq_rel))
		{
			m_onEventsReady();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
SRefreshState& CQueryPump::GetRefresh(Query::EGame game)
{
	return m_refreshes[static_cast<size_t>(game)];
}

//////////////////////////////////////////////////////////////////////////
SRefreshState const& CQueryPump::GetRefresh(Query::EGame game) const
{
	return m_refreshes[static_cast<size_t>(game)];
}

//////////////////////////////////////////////////////////////////////////
Script::CProtocolScript& CQueryPump::GetScript(Query::EGame game)
{
	return m_scripts[static_cast<size_t>(GetGame(game).protocol)];
}

//////////////////////////////////////////////////////////////////////////
Query::SGameDefinition const& CQueryPump::GetGame(Query::EGame game) const
{
	return m_catalog.games[static_cast<size_t>(game)];
}
} // namespace Lkt::Net
