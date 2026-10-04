#pragma once

#include "answered_request.hpp"
#include "conversation_record.hpp"
#include "dns_lookup.hpp"
#include "master_id.hpp"
#include "master_outcome.hpp"
#include "master_query.hpp"
#include "master_tracker.hpp"
#include "refresh_state.hpp"
#include "request_scheduler.hpp"
#include "server_request.hpp"
#include "udp_socket.hpp"
#include "net/query_event.hpp"
#include "query/game.hpp"
#include "query/server_address.hpp"
#include "script/conversation_kind.hpp"
#include "script/protocol_script.hpp"
#include <tge/non_copyable.hpp>
#include <tge/threading/event_loop.hpp>
#include <tge/threading/mpsc_queue.hpp>
#include <tge/threading/timer_id.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lkt::Net
{
// The engine's work, all on one tge event loop: commands are posted to it, replies wake it through the socket, and
// a single timer brings it back for the next deadline.
class CQueryPump final : private Tge::SNoCopyNoMove
{
public:

	CQueryPump() = default;
	~CQueryPump() = default;

	bool Initialize(std::function<void()> onEventsReady);
	void Terminate();

	void Refresh(Query::EGame game, std::vector<Query::SServerAddress> favourites, uint32_t refreshId);
	void RefreshServer(Query::EGame game, Query::SServerAddress const& address, uint32_t refreshId);
	void Cancel(Query::EGame game);
	void TakeEvents(std::vector<SQueryEvent>& events);

private:

	void Update(Clock::time_point now);
	void ArmTimer(Clock::time_point now);
	void StartRefresh(Query::EGame game, std::span<Query::SServerAddress const> favourites, uint32_t refreshId, Clock::time_point now);
	void StartServerRefresh(Query::EGame game, Query::SServerAddress const& address, uint32_t refreshId, Clock::time_point now);
	void CancelRefresh(Query::EGame game);
	void ResolveMasters(Query::EGame game, uint32_t generation, Clock::time_point now);
	void CollectLookups(Clock::time_point now);
	void AbandonLookups();
	void UpdateMasters(Clock::time_point now);
	void SendDueRequests(Clock::time_point now);
	void ExpireRequests(Clock::time_point now);
	void FinishRefreshes();
	void ReceiveDatagrams();
	void CountReceiveError(int error);
	void ReadMasterDatagram(SMasterId const& master, std::span<std::byte const> datagram);
	void ReadStatusDatagram(SAnsweredRequest const& answered, std::span<std::byte const> datagram);
	void ListServers(Query::EGame game, std::span<Query::SServerAddress const> servers);
	std::expected<SConversationRecord const*, std::string> OpenConversation(std::unordered_map<uint64_t, SConversationRecord>& conversations, uint64_t key,
		Query::EGame game, Script::EConversationKind kind);
	void CloseConversation(std::unordered_map<uint64_t, SConversationRecord>& conversations, uint64_t key);
	void CloseConversations(std::unordered_map<uint64_t, SConversationRecord>& conversations, Query::EGame game);
	std::expected<void, int> Send(Query::SServerAddress const& address, std::span<std::vector<std::byte> const> datagrams) const;
	void ReportRefresh(Query::EGame game) const;
	void Emit(SQueryEvent event);
	void NotifyIfNeeded();
	SRefreshState& GetRefresh(Query::EGame game);
	Script::CProtocolScript& GetScript(Query::EGame game);
	SRefreshState const& GetRefresh(Query::EGame game) const;

	std::atomic<bool> m_isNotified{ false };
	bool m_hasNewEvents{ false };
	std::function<void()> m_onEventsReady;

	Tge::Threading::CEventLoop m_loop;
	Tge::Threading::STimerId m_timer;
	CUdpSocket m_socket;
	Tge::Threading::CMpscQueue<SQueryEvent> m_events;

	std::vector<std::unique_ptr<SDnsLookup>> m_lookups;
	CRequestScheduler m_scheduler;
	CMasterTracker m_masters;
	std::vector<SRefreshState> m_refreshes;
	std::vector<Script::CProtocolScript> m_scripts;
	std::unordered_map<uint64_t, SConversationRecord> m_serverConversations;
	std::unordered_map<uint64_t, SConversationRecord> m_masterConversations;

	std::vector<std::byte> m_buffer;
	std::vector<SMasterQuery> m_masterQueries;
	std::vector<SMasterOutcome> m_masterOutcomes;
	std::vector<SServerRequest> m_requests;
	std::vector<Query::SServerAddress> m_listed;
};
} // namespace Lkt::Net
