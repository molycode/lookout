#pragma once

#include "fetch_state.hpp"
#include "host_lookup.hpp"
#include "http_response_reader.hpp"
#include "stream_socket.hpp"
#include "tls_connection.hpp"
#include "net/fetch_request.hpp"
#include "net/fetch_result.hpp"
#include "net/https_origin.hpp"
#include "query/server_address.hpp"
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <tge/non_copyable.hpp>
#include <tge/threading/event_loop.hpp>
#include <tge/threading/mpsc_queue.hpp>
#include <tge/threading/timer_id.hpp>
#include <tge/threading/watch_id.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Lkt::Net
{
// The fetcher's work, all on one tge event loop. The loop watches only for readable sockets, so a connect in progress
// and a send buffer that is full are polled on a short timer instead.
class CFetchPump final : private Tge::SNoCopyNoMove
{
public:

	CFetchPump() = default;
	~CFetchPump() = default;

	bool Initialize(SHttpsOrigin origin, std::function<void()> onResults);
	void Terminate();

	void Fetch(std::vector<SFetchRequest> requests);
	void Cancel();
	void TakeResults(std::vector<SFetchResult>& results);

private:

	void Advance();
	void StartRequest();
	void Resolve();
	void CollectLookup();
	void Connect(Query::SServerAddress const& address);
	void CheckConnected();
	void Handshake();
	void Send();
	void Receive();
	void Answer(std::expected<std::string, std::string> body);
	void FailAll(std::string const& reason);
	void CloseConnection();
	void AbandonLookup();
	void PollIn(std::chrono::milliseconds delay);
	void OnDeadline();
	std::string Explain(std::string_view reason) const;

	SHttpsOrigin m_origin;
	std::function<void()> m_onResults;
	std::atomic<bool> m_isNotified{ false };
	std::string m_setUpError;

	Tge::Threading::CEventLoop m_loop;
	Tge::Threading::CMpscQueue<SFetchResult> m_results;
	mbedtls_ssl_config m_config{};
	mbedtls_x509_crt m_caChain{};
	bool m_isTlsReady{ false };

	std::deque<SFetchRequest> m_requests;
	EFetchState m_state{ EFetchState::Idle };
	std::unique_ptr<SHostLookup> m_pLookup;
	CStreamSocket m_socket;
	CTlsConnection m_tls;
	std::optional<Tge::Threading::SWatchId> m_watch;
	std::optional<Tge::Threading::STimerId> m_pollTimer;
	std::optional<Tge::Threading::STimerId> m_deadlineTimer;
	CHttpResponseReader m_reader;
	std::string m_request;
	size_t m_numSent{ 0 };
	std::vector<std::byte> m_buffer;
	bool m_isReused{ false };
	bool m_hasRetried{ false };
	bool m_hasReceived{ false };
};
} // namespace Lkt::Net
