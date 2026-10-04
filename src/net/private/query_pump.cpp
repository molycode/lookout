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
	m_serverConversations.clear();
	m_masterConversations.clear();

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
	ExpireRequests(now);
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
	CloseConversations(m_serverConversations, game);
	CloseConversations(m_masterConversations, game);
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
	CloseConversations(m_serverConversations, game);
	CloseConversations(m_masterConversations, game);

	if (refresh.isActive)
	{
		refresh.isActive = false;
		Emit(SRefreshFinished{ game });
	}
}

//////////////////////////////////////////////////////////////////////////
// Polled, not notified: a notification would run on a glibc thread and race the loop.
void CQueryPump::ResolveMasters(Query::EGame game, uint32_t generation, Clock::time_point now)
{
	std::span<Query::SMasterEndpoint const> const masters{ Query::GetGame(game).masters };

	for (size_t index{ 0 }; index < masters.size(); ++index)
	{
		auto lookup{ std::make_unique<SDnsLookup>(game, generation, index, std::string{ masters[index].host }) };

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
		Query::SGameDefinition const& game{ Query::GetGame(query.master.game) };
		std::expected<SConversationRecord const*, std::string> const conversation{ OpenConversation(m_masterConversations,
			ToConversationKey(query.master), query.master.game, Script::EConversationKind::Master) };

		if (conversation.has_value())
		{
			std::expected<void, int> const sent{ Send(query.address, (*conversation)->send) };

			if (!sent.has_value())
			{
				gLog.Warning("{}: cannot ask master {}: {}", game.name, Query::FormatAddress(query.address), std::strerror(sent.error()));
			}
		}
		else
		{
			m_masters.Fail(query.master, std::format("cannot be asked: {}", conversation.error()), m_masterOutcomes);
		}
	}

	for (SMasterOutcome const& outcome : m_masterOutcomes)
	{
		CloseConversation(m_masterConversations, ToConversationKey(outcome.master));

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
void CQueryPump::SendDueRequests(Clock::time_point now)
{
	m_scheduler.TakeDue(now, m_requests);

	for (SServerRequest const& request : m_requests)
	{
		uint64_t const key{ ToConversationKey(request.game, request.address) };

		TGE_ASSERT(m_serverConversations.contains(key) == (request.numAttempts > 1), "A server's conversation must span exactly its attempts");

		std::expected<SConversationRecord const*, std::string> const conversation{ OpenConversation(m_serverConversations, key, request.game,
			Script::EConversationKind::Server) };
		SRefreshStats& stats{ GetRefresh(request.game).stats };

		if (!conversation.has_value())
		{
			RecordScriptFailure(stats, conversation.error());
			m_scheduler.Abandon(request);
			Emit(SServerFailed{ request.game, request.address, EServerFailure::BadReply });
		}
		else if (std::expected<void, int> const sent{ Send(request.address, (*conversation)->send) }; !sent.has_value())
		{
			if (stats.numSendFailures == 0)
			{
				stats.firstSendFailure = request.address;
				stats.firstSendError = sent.error();
			}

			++stats.numSendFailures;
		}
	}

	m_requests.clear();
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::ExpireRequests(Clock::time_point now)
{
	m_scheduler.TakeExpired(now, m_requests);

	for (SServerRequest const& request : m_requests)
	{
		SRefreshStats& stats{ GetRefresh(request.game).stats };

		CloseConversation(m_serverConversations, ToConversationKey(request.game, request.address));
		stats.firstNoAnswer = (stats.numNoAnswer == 0) ? request.address : stats.firstNoAnswer;
		++stats.numNoAnswer;
		Emit(SServerFailed{ request.game, request.address, EServerFailure::NoAnswer });
	}

	m_requests.clear();
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
				&& std::ranges::none_of(m_masterConversations, [game](auto const& entry) { return entry.second.game == game; }),
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
			std::optional<SMasterId> const master{ m_masters.OnDatagram(source, now) };

			if (master.has_value())
			{
				ReadMasterDatagram(*master, datagram);
			}
			else
			{
				std::optional<SAnsweredRequest> const answered{ m_scheduler.Answer(source, now) };

				if (answered.has_value())
				{
					ReadStatusDatagram(answered.value(), datagram);
				}
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
void CQueryPump::ReadMasterDatagram(SMasterId const& master, std::span<std::byte const> datagram)
{
	SRefreshStats& stats{ GetRefresh(master.game).stats };
	auto const it{ m_masterConversations.find(ToConversationKey(master)) };

	TGE_ASSERT(it != m_masterConversations.end(), "A datagram from a master that was never asked");

	std::expected<Script::SScriptAction, std::string> const action{ GetScript(master.game).Receive(it->second.conversation, datagram) };
	std::span<Query::SServerAddress const> const servers{ action.has_value() ? std::span<Query::SServerAddress const>{ action->servers }
		: std::span<Query::SServerAddress const>{} };
	std::optional<Query::EParseError> const error{ action.has_value() ? action->reason : Query::EParseError::ScriptFailed };

	if (!action.has_value())
	{
		RecordScriptFailure(stats, action.error());
	}

	if (error.has_value())
	{
		stats.firstBadMasterDatagramError = (stats.numBadMasterDatagrams == 0) ? *error : stats.firstBadMasterDatagramError;
		++stats.numBadMasterDatagrams;
	}

	size_t const numAdmitted{ m_masters.AdmitEntries(master, servers.size()) };

	stats.numCappedEntries += servers.size() - numAdmitted;

	for (Query::SServerAddress const& address : servers.first(numAdmitted))
	{
		if (Query::IsQueryable(address))
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
}

//////////////////////////////////////////////////////////////////////////
// One datagram is all this engine takes, so a reply the script still waits on is finished right away.
void CQueryPump::ReadStatusDatagram(SAnsweredRequest const& answered, std::span<std::byte const> datagram)
{
	Query::EGame const game{ answered.request.game };
	SRefreshStats& stats{ GetRefresh(game).stats };
	Script::CProtocolScript& script{ GetScript(game) };
	auto const it{ m_serverConversations.find(ToConversationKey(game, answered.request.address)) };

	TGE_ASSERT(it != m_serverConversations.end(), "An answered request without a conversation");

	std::expected<Script::SScriptAction, std::string> action{ script.Receive(it->second.conversation, datagram) };

	if (action.has_value() && !action->reply.has_value() && !action->reason.has_value())
	{
		action = script.Finish(it->second.conversation);
	}

	script.End(it->second.conversation);
	m_serverConversations.erase(it);

	if (action.has_value() && action->reply.has_value())
	{
		uint32_t const pingMs{ static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(answered.roundTrip).count()) };

		++stats.numAnswered;
		stats.numMalformedPlayerLines += action->reply->numMalformedPlayerLines;
		Emit(SServerAnswered{ game, answered.request.address, pingMs, std::move(*action->reply) });
	}
	else
	{
		Query::EParseError const error{ action.has_value() ? action->reason.value_or(Query::EParseError::Truncated) : Query::EParseError::ScriptFailed };

		if (!action.has_value())
		{
			RecordScriptFailure(stats, action.error());
		}

		stats.firstBadReply = (stats.numBadReplies == 0) ? answered.request.address : stats.firstBadReply;
		stats.firstBadReplyError = (stats.numBadReplies == 0) ? error : stats.firstBadReplyError;
		++stats.numBadReplies;
		Emit(SServerFailed{ game, answered.request.address, EServerFailure::BadReply });
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
// A retry finds its conversation open and resends what it started with.
std::expected<SConversationRecord const*, std::string> CQueryPump::OpenConversation(std::unordered_map<uint64_t, SConversationRecord>& conversations,
	uint64_t key, Query::EGame game, Script::EConversationKind kind)
{
	auto const it{ conversations.find(key) };
	std::expected<SConversationRecord const*, std::string> result{ (it != conversations.end()) ? &it->second : nullptr };

	if (it == conversations.end())
	{
		SConversationRecord record{ game, Script::SConversation{ kind, 0 }, {} };
		std::expected<Script::SScriptAction, std::string> started{ GetScript(game).Start(record.conversation, Query::GetGame(game).protocolOptions) };

		if (started.has_value())
		{
			record.send = std::move(started->send);
			result = &conversations.emplace(key, std::move(record)).first->second;
		}
		else
		{
			result = std::unexpected{ std::move(started.error()) };
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::CloseConversation(std::unordered_map<uint64_t, SConversationRecord>& conversations, uint64_t key)
{
	auto const it{ conversations.find(key) };

	if (it != conversations.end())
	{
		GetScript(it->second.game).End(it->second.conversation);
		conversations.erase(it);
	}
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::CloseConversations(std::unordered_map<uint64_t, SConversationRecord>& conversations, Query::EGame game)
{
	for (auto it{ conversations.begin() }; it != conversations.end();)
	{
		if (it->second.game == game)
		{
			GetScript(game).End(it->second.conversation);
			it = conversations.erase(it);
		}
		else
		{
			++it;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Every datagram is sent; the first failure is the one reported.
std::expected<void, int> CQueryPump::Send(Query::SServerAddress const& address, std::span<std::vector<std::byte> const> datagrams) const
{
	std::expected<void, int> result{};

	for (std::vector<std::byte> const& datagram : datagrams)
	{
		std::expected<void, int> const sent{ m_socket.Send(address, datagram) };

		if (result.has_value() && !sent.has_value())
		{
			result = sent;
		}
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

	if (stats.numCappedEntries > 0)
	{
		gLog.Warning("{}: a master listed {} more servers than one master may; they were ignored", name, stats.numCappedEntries);
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
