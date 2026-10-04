#pragma once

#include "answered_request.hpp"
#include "conversation_record.hpp"
#include "dns_lookup.hpp"
#include "ended_request.hpp"
#include "master_conversation.hpp"
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
#include "script/script_action.hpp"
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
	void Resolve(Query::EGame game, uint32_t generation, size_t index, Clock::time_point now);
	void CollectLookups(Clock::time_point now);
	void AbandonLookups();
	void UpdateMasters(Clock::time_point now);
	std::expected<void, std::string> OpenMaster(SMasterQuery const& query);
	std::expected<void, std::string> SendToMaster(SMasterConversation const& conversation) const;
	void ReceiveFromMaster(SMasterId const& master);
	void ReadMasterData(SMasterId const& master, SMasterConversation& conversation, std::span<std::byte const> data, Clock::time_point now);
	void CloseMaster(uint64_t key);
	void SendDueRequests(Clock::time_point now);
	void SendToServer(SServerRequest const& request, SConversationRecord const& record);
	void EndRequests(Clock::time_point now);
	void ReceiveDatagrams();
	void CountReceiveError(int error);
	void ReadStatusDatagram(SAnsweredRequest const& answered, std::span<std::byte const> datagram, Clock::time_point now);
	void FinishServer(SServerRequest const& request, Clock::duration roundTrip);
	void EndServer(SServerRequest const& request, Clock::duration roundTrip, std::expected<Script::SScriptAction, std::string> action);
	void CloseServer(SServerRequest const& request);
	void CloseConversations(Query::EGame game);
	std::expected<void, std::string> StartConversation(SConversationRecord& record);
	void FinishRefreshes();
	void ListServers(Query::EGame game, std::span<Query::SServerAddress const> servers);
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
	std::unordered_map<uint64_t, SMasterConversation> m_masterConversations;

	std::vector<std::byte> m_buffer;
	std::vector<SMasterQuery> m_masterQueries;
	std::vector<SMasterOutcome> m_masterOutcomes;
	std::vector<SServerRequest> m_requests;
	std::vector<SEndedRequest> m_endedRequests;
	std::vector<Query::SServerAddress> m_listed;
};
} // namespace Lkt::Net
