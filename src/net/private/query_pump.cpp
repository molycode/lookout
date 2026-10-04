#include "query_pump.hpp"
#include "loggers.hpp"
#include "query/game_catalog.hpp"
#include "query/protocol_definition.hpp"
#include <tge/assert.hpp>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace Lkt::Net
{
namespace
{
constexpr size_t MaxDatagramSize{ 65536 };
constexpr size_t MaxDatagramsPerWake{ 256 };
constexpr std::chrono::milliseconds LookupPollInterval{ 20 };

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
void FreeResults(gaicb& request)
{
	if (request.ar_result != nullptr)
	{
		freeaddrinfo(request.ar_result);
		request.ar_result = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
std::expected<uint32_t, std::string> TakeAddress(gaicb& request, int status)
{
	std::expected<uint32_t, std::string> result{ std::unexpected{ std::string{ gai_strerror(status) } } };

	if (status == 0 && request.ar_result != nullptr)
	{
		result = ntohl(reinterpret_cast<sockaddr_in const*>(request.ar_result->ai_addr)->sin_addr.s_addr);
	}

	FreeResults(request);

	return result;
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
bool CQueryPump::Initialize(std::function<void()> onEventsReady)
{
	m_onEventsReady = std::move(onEventsReady);
	m_buffer.resize(MaxDatagramSize);
	m_refreshes.assign(Query::GetGameCatalog().size(), SRefreshState{});

	std::span<Query::SProtocolDefinition const> const protocols{ Query::GetProtocolCatalog() };
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

	bool const isReady{ areScriptsReady && m_socket.Initialize() && m_loop.Initialize("LookoutNet") };

	if (isReady)
	{
		m_loop.Post([this]()
		{
			std::optional<Tge::Threading::SWatchId> const watch{ m_loop.Watch(m_socket.GetDescriptor(), [this]()
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
		conversation.socket.Terminate();
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

		StartRefresh(game, favourites, refreshId, now);
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
void CQueryPump::StartRefresh(Query::EGame game, std::span<Query::SServerAddress const> favourites, uint32_t refreshId, Clock::time_point now)
{
	SRefreshState& refresh{ GetRefresh(game) };
	uint32_t const generation{ refreshId };

	TGE_ASSERT(generation > refresh.generation, "A refresh id that is not newer than the game's last one");

	m_scheduler.Cancel(game);
	CloseConversations(game);
	refresh = SRefreshState{};
	refresh.isActive = true;
	refresh.generation = generation;
	refresh.startedAt = now;

	m_masters.Begin(game, generation, Query::GetGame(game).masters, now);
	ResolveMasters(game, generation, now);
	ListServers(game, favourites);
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::StartServerRefresh(Query::EGame game, Query::SServerAddress const& address, uint32_t refreshId, Clock::time_point now)
{
	SRefreshState& refresh{ GetRefresh(game) };

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
	std::span<Query::SMasterEndpoint const> const masters{ Query::GetGame(game).masters };

	for (size_t index{ 0 }; index < masters.size(); ++index)
	{
		in_addr literal{};

		if (inet_pton(AF_INET, masters[index].host.c_str(), &literal) == 1)
		{
			m_masters.OnResolved(game, generation, index, ntohl(literal.s_addr), now);
		}
		else
		{
			Resolve(game, generation, index, now);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Polled, not notified: a notification would run on a glibc thread and race the loop.
void CQueryPump::Resolve(Query::EGame game, uint32_t generation, size_t index, Clock::time_point now)
{
	auto lookup{ std::make_unique<SDnsLookup>(game, generation, index, std::string{ Query::GetGame(game).masters[index].host }) };

	lookup->hints.ai_family = AF_INET;
	lookup->hints.ai_socktype = SOCK_DGRAM;
	lookup->request.ar_name = lookup->host.c_str();
	lookup->request.ar_request = &lookup->hints;

	std::array<gaicb*, 1> requests{ &lookup->request };
	int const status{ getaddrinfo_a(GAI_NOWAIT, requests.data(), static_cast<int>(requests.size()), nullptr) };

	if (status == 0)
	{
		m_lookups.emplace_back(std::move(lookup));
	}
	else
	{
		m_masters.OnResolved(game, generation, index, std::unexpected{ std::string{ gai_strerror(status) } }, now);
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::CollectLookups(Clock::time_point now)
{
	std::erase_if(m_lookups, [this, now](std::unique_ptr<SDnsLookup> const& lookup)
	{
		int const status{ gai_error(&lookup->request) };
		bool const isDone{ status != EAI_INPROGRESS };

		if (isDone)
		{
			m_masters.OnResolved(lookup->game, lookup->generation, lookup->masterIndex, TakeAddress(lookup->request, status), now);
		}

		return isDone;
	});
}

//////////////////////////////////////////////////////////////////////////
// A lookup glibc cannot cancel still writes into its request, so that request is left to glibc, never freed.
void CQueryPump::AbandonLookups()
{
	for (std::unique_ptr<SDnsLookup>& lookup : m_lookups)
	{
		if (gai_cancel(&lookup->request) == EAI_NOTCANCELED)
		{
			lookup.release();
		}
		else
		{
			FreeResults(lookup->request);
		}
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
		CloseMaster(ToConversationKey(outcome.master));

		if (!outcome.failure.empty())
		{
			gLog.Warning("{}: master {} {}", Query::GetGame(outcome.master.game).name, outcome.host, outcome.failure);
			Emit(SMasterFailed{ outcome.master.game, std::string{ outcome.host }, outcome.failure });
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

	if (!conversation.socket.Initialize())
	{
		result = std::unexpected{ std::string{ "cannot be asked: no socket can be created for it" } };
	}
	else if (std::expected<void, int> const connected{ conversation.socket.Connect(query.address) }; !connected.has_value())
	{
		result = std::unexpected{ std::format("cannot be asked: {}", std::strerror(connected.error())) };
	}
	else
	{
		conversation.watch = m_loop.Watch(conversation.socket.GetDescriptor(), [this, master = query.master]()
		{
			ReceiveMasterDatagrams(master);
			Update(Clock::now());
		});

		result = conversation.watch.has_value() ? StartConversation(conversation.record)
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

	for (std::vector<std::byte> const& datagram : conversation.record.send)
	{
		std::expected<void, int> const sent{ conversation.socket.Send(conversation.address, datagram) };

		if (result.has_value() && !sent.has_value())
		{
			result = std::unexpected{ std::format("cannot be asked: {}", std::strerror(sent.error())) };
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Stops at a master that ended meanwhile; its outcome, handled by the update after this, closes the socket.
void CQueryPump::ReceiveMasterDatagrams(SMasterId const& master)
{
	uint64_t const key{ ToConversationKey(master) };
	bool isDraining{ true };
	size_t numReceived{ 0 };

	while (isDraining && numReceived < MaxDatagramsPerWake)
	{
		auto const it{ m_masterConversations.find(key) };

		isDraining = it != m_masterConversations.end() && m_masters.IsAsking(master);

		if (isDraining)
		{
			Query::SServerAddress source{};
			std::expected<size_t, int> const received{ it->second.socket.Receive(m_buffer, source) };

			if (received.has_value())
			{
				ReadMasterDatagram(master, it->second, std::span<std::byte const>{ m_buffer.data(), *received }, Clock::now());
				++numReceived;
			}
			else
			{
				isDraining = false;

				if (received.error() != EAGAIN && received.error() != EWOULDBLOCK)
				{
					m_masters.Fail(master, (received.error() == ECONNREFUSED) ? std::string{ "refused the query" }
						: std::format("cannot be read: {}", std::strerror(received.error())), m_masterOutcomes);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// A failing script ends the master: what it would make of the rest cannot be trusted.
void CQueryPump::ReadMasterDatagram(SMasterId const& master, SMasterConversation& conversation, std::span<std::byte const> datagram, Clock::time_point now)
{
	SRefreshStats& stats{ GetRefresh(master.game).stats };

	m_masters.OnDatagram(master, now);

	std::expected<Script::SScriptAction, std::string> action{ GetScript(master.game).Receive(conversation.record.conversation, datagram) };

	if (!action.has_value())
	{
		m_masters.Fail(master, std::format("cannot be read: {}", action.error()), m_masterOutcomes);
	}
	else
	{
		size_t const numAdmitted{ m_masters.AdmitEntries(master, action->servers.size()) };

		if (!IsEmpty(*action))
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

		ListServers(master.game, m_listed);
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

		it->second.socket.Terminate();
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
			started = StartConversation(it->second);
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
		std::expected<void, int> const sent{ m_socket.Send(request.address, datagram) };

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
			stats.firstNoAnswer = (stats.numNoAnswer == 0) ? ended.request.address : stats.firstNoAnswer;
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
			ReportRefresh(game);
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
		Query::SServerAddress source{};
		std::expected<size_t, int> const received{ m_socket.Receive(m_buffer, source) };

		if (received.has_value())
		{
			Clock::time_point const now{ Clock::now() };
			std::span<std::byte const> const datagram{ m_buffer.data(), received.value() };
			std::optional<SAnsweredRequest> const answered{ m_scheduler.OnDatagram(source, datagram.size(), now) };

			if (answered.has_value())
			{
				ReadStatusDatagram(answered.value(), datagram, now);
			}

			++numReceived;
		}
		else
		{
			isDraining = false;

			if (received.error() != EAGAIN && received.error() != EWOULDBLOCK)
			{
				CountReceiveError(received.error());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Not tied to one server, so every running refresh reports it.
void CQueryPump::CountReceiveError(int error)
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
std::expected<void, std::string> CQueryPump::StartConversation(SConversationRecord& record)
{
	std::expected<Script::SScriptAction, std::string> started{ GetScript(record.game).Start(record.conversation, Query::GetGame(record.game).protocolOptions) };
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
void CQueryPump::ReportRefresh(Query::EGame game) const
{
	SRefreshState const& refresh{ GetRefresh(game) };
	SRefreshStats const& stats{ refresh.stats };
	std::string_view const name{ Query::GetGame(game).name };
	double const seconds{ std::chrono::duration<double>{ Clock::now() - refresh.startedAt }.count() };

	gLog.Info("{}: {} of {} servers answered in {:.1f} s", name, stats.numAnswered, stats.numListed, seconds);

	if (stats.numNoAnswer > 0)
	{
		gLog.Warning("{}: {} servers did not answer, first {}", name, stats.numNoAnswer, Query::FormatAddress(stats.firstNoAnswer));
	}

	if (stats.numBadReplies > 0)
	{
		gLog.Warning("{}: {} status replies were unreadable, first from {} ({})", name, stats.numBadReplies, Query::FormatAddress(stats.firstBadReply), Query::ToString(stats.firstBadReplyError));
	}

	if (stats.numScriptFailures > 0)
	{
		gLog.Warning("{}: the protocol script failed {} times, first: {}", name, stats.numScriptFailures, stats.firstScriptFailure);
	}

	if (stats.numMalformedPlayerLines > 0)
	{
		gLog.Warning("{}: {} player lines were unreadable", name, stats.numMalformedPlayerLines);
	}

	if (stats.numUnqueryable > 0)
	{
		gLog.Warning("{}: masters listed {} addresses no public server can have, first {}; they were not asked", name, stats.numUnqueryable, Query::FormatAddress(stats.firstUnqueryable));
	}

	if (stats.numBadMasterDatagrams > 0)
	{
		gLog.Warning("{}: {} master datagrams were unreadable ({}); their complete entries were kept", name, stats.numBadMasterDatagrams, Query::ToString(stats.firstBadMasterDatagramError));
	}

	if (stats.numOverCap > 0)
	{
		gLog.Warning("{}: {} servers sent more than one server may; each was cut short, first {}", name, stats.numOverCap, Query::FormatAddress(stats.firstOverCap));
	}

	if (stats.numSendFailures > 0)
	{
		gLog.Warning("{}: {} status requests could not be sent, first to {}: {}", name, stats.numSendFailures, Query::FormatAddress(stats.firstSendFailure), std::strerror(stats.firstSendError));
	}

	if (stats.numReceiveErrors > 0)
	{
		gLog.Warning("{}: receiving failed {} times: {}", name, stats.numReceiveErrors, std::strerror(stats.firstReceiveError));
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
	return m_scripts[static_cast<size_t>(Query::GetGame(game).protocol)];
}
} // namespace Lkt::Net
