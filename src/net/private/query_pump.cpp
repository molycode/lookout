#include "query_pump.hpp"
#include "loggers.hpp"
#include "query/game_catalog.hpp"
#include "query/protocol.hpp"
#include <tge/assert.hpp>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <expected>
#include <optional>
#include <string>
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
std::vector<std::byte> const& GetStatusRequest(std::array<std::vector<std::byte>, Query::NumProtocolFamilies> const& requests, Query::EGame game)
{
	return requests[static_cast<size_t>(Query::GetGame(game).family)];
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CQueryPump::Initialize(std::function<void()> onEventsReady)
{
	m_onEventsReady = std::move(onEventsReady);
	m_buffer.resize(MaxDatagramSize);
	m_refreshes.assign(Query::GetGameCatalog().size(), SRefreshState{});

	for (size_t family{ 0 }; family < Query::NumProtocolFamilies; ++family)
	{
		m_statusRequests[family] = Query::GetProtocol(static_cast<Query::EProtocolFamily>(family)).StatusRequest();
	}

	bool const isReady{ m_socket.Initialize() && m_loop.Initialize("LookoutNet") };

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
		Query::SGameDefinition const& game{ Query::GetGame(query.game) };
		std::expected<void, int> const sent{ m_socket.Send(query.address, Query::GetProtocol(game.family).MasterRequest(game)) };

		if (!sent.has_value())
		{
			gLog.Warning("{}: cannot ask master {}: {}", game.name, Query::FormatAddress(query.address), std::strerror(sent.error()));
		}
	}

	for (SMasterOutcome const& outcome : m_masterOutcomes)
	{
		if (!outcome.failure.empty())
		{
			gLog.Warning("{}: master {} {}", Query::GetGame(outcome.game).name, outcome.host, outcome.failure);
			Emit(SMasterFailed{ outcome.game, std::string{ outcome.host }, outcome.failure });
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
		std::expected<void, int> const sent{ m_socket.Send(request.address, GetStatusRequest(m_statusRequests, request.game)) };

		if (!sent.has_value())
		{
			SRefreshStats& stats{ GetRefresh(request.game).stats };

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
			std::optional<Query::EGame> const masterGame{ m_masters.OnDatagram(source, now) };

			if (masterGame.has_value())
			{
				ReadMasterDatagram(masterGame.value(), source, datagram);
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
void CQueryPump::ReadMasterDatagram(Query::EGame game, Query::SServerAddress const& source, std::span<std::byte const> datagram)
{
	SRefreshStats& stats{ GetRefresh(game).stats };
	std::expected<void, Query::EParseError> const parsed{ Query::GetProtocol(Query::GetGame(game).family).ParseMasterReply(datagram, m_entries) };

	if (!parsed.has_value())
	{
		stats.firstBadMasterDatagramError = (stats.numBadMasterDatagrams == 0) ? parsed.error() : stats.firstBadMasterDatagramError;
		++stats.numBadMasterDatagrams;
	}

	size_t const numAdmitted{ m_masters.AdmitEntries(source, m_entries.size()) };

	stats.numCappedEntries += m_entries.size() - numAdmitted;

	for (size_t index{ 0 }; index < numAdmitted; ++index)
	{
		Query::SServerAddress const& address{ m_entries[index] };

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

	ListServers(game, m_listed);
	m_entries.clear();
	m_listed.clear();
}

//////////////////////////////////////////////////////////////////////////
void CQueryPump::ReadStatusDatagram(SAnsweredRequest const& answered, std::span<std::byte const> datagram)
{
	Query::EGame const game{ answered.request.game };
	SRefreshStats& stats{ GetRefresh(game).stats };
	std::expected<Query::SStatusReply, Query::EParseError> reply{ Query::GetProtocol(Query::GetGame(game).family).ParseStatusReply(datagram) };

	if (reply.has_value())
	{
		uint32_t const pingMs{ static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(answered.roundTrip).count()) };

		++stats.numAnswered;
		stats.numMalformedPlayerLines += reply->numMalformedPlayerLines;
		Emit(SServerAnswered{ game, answered.request.address, pingMs, std::move(reply.value()) });
	}
	else
	{
		stats.firstBadReply = (stats.numBadReplies == 0) ? answered.request.address : stats.firstBadReply;
		stats.firstBadReplyError = (stats.numBadReplies == 0) ? reply.error() : stats.firstBadReplyError;
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
} // namespace Lkt::Net
