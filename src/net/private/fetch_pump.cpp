#include "fetch_pump.hpp"
#include "ca_certificates.hpp"
#include "endpoint.hpp"
#include "loggers.hpp"
#include <psa/crypto.h>
#include <cstdint>
#include <format>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace Lkt::Net
{
namespace
{
constexpr std::chrono::milliseconds LookupPoll{ 20 };
constexpr std::chrono::milliseconds SocketPoll{ 10 };
constexpr size_t ReadSize{ 16 * 1024 };
constexpr uint16_t HttpsPort{ 443 };

//////////////////////////////////////////////////////////////////////////
std::string MakeRequest(SHttpsOrigin const& origin, std::string_view path)
{
	std::string const host{ (origin.port == HttpsPort) ? origin.host : std::format("{}:{}", origin.host, origin.port) };

	return std::format("GET {} HTTP/1.1\r\nHost: {}\r\nUser-Agent: {}\r\nAccept-Encoding: identity\r\nConnection: keep-alive\r\n\r\n", path, host,
		origin.userAgent);
}

//////////////////////////////////////////////////////////////////////////
bool IsWaiting(int error)
{
	return error == MBEDTLS_ERR_SSL_WANT_READ || error == MBEDTLS_ERR_SSL_WANT_WRITE;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CFetchPump::Initialize(SHttpsOrigin origin, std::function<void()> onResults)
{
	m_origin = std::move(origin);
	m_onResults = std::move(onResults);
	m_buffer.resize(ReadSize);
	mbedtls_ssl_config_init(&m_config);
	mbedtls_x509_crt_init(&m_caChain);
	m_isTlsReady = true;

	bool const isCryptoReady{ psa_crypto_init() == PSA_SUCCESS };
	int const defaults{ isCryptoReady ? mbedtls_ssl_config_defaults(&m_config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)
		: MBEDTLS_ERR_SSL_BAD_CONFIG };

	if (defaults == 0)
	{
		std::expected<void, std::string> const loaded{ LoadCaCertificates(m_caChain, m_origin.caFile) };

		mbedtls_ssl_conf_authmode(&m_config, MBEDTLS_SSL_VERIFY_REQUIRED);
		mbedtls_ssl_conf_min_tls_version(&m_config, MBEDTLS_SSL_VERSION_TLS1_2);
		mbedtls_ssl_conf_ca_chain(&m_config, &m_caChain, nullptr);

		if (!loaded.has_value())
		{
			m_setUpError = std::format("Lookout {}", loaded.error());
			gLog.Warning("Nothing can be downloaded from {}: {}", m_origin.host, m_setUpError);
		}
	}
	else
	{
		gLog.Error("Cannot set up TLS to download from {}", m_origin.host);
	}

	return defaults == 0 && m_loop.Initialize("LookoutFetch");
}

//////////////////////////////////////////////////////////////////////////
// The loop is gone by now, so the connection is closed without it.
void CFetchPump::Terminate()
{
	m_loop.Terminate();
	m_tls.Terminate();
	m_socket.Terminate();
	m_lookup.Terminate();
	m_watch.reset();
	m_pollTimer.reset();
	m_deadlineTimer.reset();

	if (m_isTlsReady)
	{
		mbedtls_ssl_config_free(&m_config);
		mbedtls_x509_crt_free(&m_caChain);
		m_isTlsReady = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Fetch(std::vector<SFetchRequest> requests)
{
	m_loop.Post([this, requests = std::move(requests)]() mutable
	{
		for (SFetchRequest& request : requests)
		{
			m_requests.emplace_back(std::move(request));
		}

		Advance();
	});
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Cancel()
{
	m_loop.Post([this]()
	{
		m_requests.clear();
		CloseConnection();

		if (m_deadlineTimer.has_value())
		{
			m_loop.CancelTimer(*m_deadlineTimer);
			m_deadlineTimer.reset();
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::TakeResults(std::vector<SFetchResult>& results)
{
	m_isNotified.exchange(false, std::memory_order_acq_rel);

	SFetchResult result{};

	while (m_results.Dequeue(result))
	{
		results.emplace_back(std::move(result));
	}
}

//////////////////////////////////////////////////////////////////////////
// Runs each step while one leads to the next; a step that must wait leaves the state as it was.
void CFetchPump::Advance()
{
	EFetchState previous{ EFetchState::Idle };

	do
	{
		previous = m_state;

		switch (m_state)
		{
			case EFetchState::Idle:
				StartRequest();
				break;
			case EFetchState::Resolving:
				CollectLookup();
				break;
			case EFetchState::Connecting:
				CheckConnected();
				break;
			case EFetchState::Handshaking:
				Handshake();
				break;
			case EFetchState::Sending:
				Send();
				break;
			case EFetchState::Receiving:
				Receive();
				break;
		}
	} while (m_state != previous);
}

//////////////////////////////////////////////////////////////////////////
// The deadline covers the connection too: a request that has to open one waits for both.
void CFetchPump::StartRequest()
{
	if (!m_requests.empty() && !m_setUpError.empty())
	{
		FailAll(m_setUpError);
	}
	else if (!m_requests.empty())
	{
		m_request = MakeRequest(m_origin, m_requests.front().path);
		m_numSent = 0;
		m_hasReceived = false;

		if (m_deadlineTimer.has_value())
		{
			m_loop.CancelTimer(*m_deadlineTimer);
		}

		m_deadlineTimer = m_loop.ScheduleAt(std::chrono::steady_clock::now() + m_origin.timeout, [this]()
		{
			m_deadlineTimer.reset();
			OnDeadline();
			Advance();
		});

		if (m_isReused)
		{
			m_state = EFetchState::Sending;
		}
		else if (m_origin.address.has_value())
		{
			Connect(*m_origin.address);
		}
		else
		{
			Resolve();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Resolve()
{
	std::expected<void, std::error_code> const started{ m_lookup.Start(m_origin.host) };

	if (started.has_value())
	{
		m_state = EFetchState::Resolving;
	}
	else
	{
		FailAll(Explain(std::format("cannot be looked up: {}", started.error().message())));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::CollectLookup()
{
	std::optional<std::expected<uint32_t, std::error_code>> const result{ m_lookup.Poll() };

	if (!result.has_value())
	{
		PollIn(LookupPoll);
	}
	else if (result->has_value())
	{
		Connect(Query::SServerAddress{ **result, m_origin.port });
	}
	else
	{
		FailAll(Explain(std::format("cannot be looked up: {}", result->error().message())));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Connect(Query::SServerAddress const& address)
{
	std::expected<void, std::error_code> const connecting{ m_socket.Initialize().and_then([this, &address]() { return m_socket.Connect(ToEndpoint(address)); }) };

	if (connecting.has_value())
	{
		m_state = EFetchState::Connecting;
	}
	else
	{
		FailAll(Explain(std::format("cannot be reached: {}", connecting.error().message())));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::CheckConnected()
{
	std::expected<bool, std::error_code> const connected{ m_socket.PollConnected() };

	if (!connected.has_value())
	{
		FailAll(Explain(std::format("cannot be reached: {}", connected.error().message())));
	}
	else if (!*connected)
	{
		PollIn(SocketPoll);
	}
	else
	{
		std::expected<void, std::string> const secured{ m_tls.Initialize(m_config, m_origin.host, m_socket) };

		m_watch = secured.has_value() ? m_loop.Watch(m_socket.GetWaitable(), [this]() { Advance(); }) : std::nullopt;

		if (!secured.has_value())
		{
			FailAll(Explain(secured.error()));
		}
		else if (!m_watch.has_value())
		{
			FailAll(Explain("cannot be listened to: the event loop refused its socket"));
		}
		else
		{
			m_state = EFetchState::Handshaking;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Handshake()
{
	std::expected<void, int> const handshake{ m_tls.Handshake() };

	if (handshake.has_value())
	{
		m_state = EFetchState::Sending;
	}
	else if (handshake.error() == MBEDTLS_ERR_SSL_WANT_WRITE)
	{
		PollIn(SocketPoll);
	}
	else if (!IsWaiting(handshake.error()))
	{
		FailAll(Explain(m_tls.Describe(handshake.error())));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Send()
{
	std::expected<size_t, int> written{ static_cast<size_t>(0) };

	while (written.has_value() && m_numSent < m_request.size())
	{
		written = m_tls.Write(std::as_bytes(std::span{ m_request }).subspan(m_numSent));
		m_numSent += written.value_or(0);
	}

	if (written.has_value())
	{
		m_reader.Reset(m_requests.front().maxSize);
		m_state = EFetchState::Receiving;
	}
	else if (written.error() == MBEDTLS_ERR_SSL_WANT_WRITE)
	{
		PollIn(SocketPoll);
	}
	else if (!IsWaiting(written.error()))
	{
		Answer(std::unexpected{ Explain(m_tls.Describe(written.error())) });
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Receive()
{
	std::expected<size_t, int> read{ m_tls.Read(m_buffer) };
	EHttpReadState state{ EHttpReadState::Headers };

	while (read.has_value() && *read > 0 && state != EHttpReadState::Complete && state != EHttpReadState::Failed)
	{
		m_hasReceived = true;
		state = m_reader.Read(std::span{ m_buffer }.first(*read));
		read = (state == EHttpReadState::Complete || state == EHttpReadState::Failed) ? read : m_tls.Read(m_buffer);
	}

	if (state == EHttpReadState::Complete)
	{
		Answer(m_reader.TakeBody());
	}
	else if (state == EHttpReadState::Failed)
	{
		Answer(std::unexpected{ Explain(m_reader.GetError()) });
	}
	else if (read.has_value())
	{
		Answer(std::unexpected{ Explain("closed the connection before its reply was complete") });
	}
	else if (read.error() == MBEDTLS_ERR_SSL_WANT_WRITE)
	{
		PollIn(SocketPoll);
	}
	else if (!IsWaiting(read.error()))
	{
		Answer(std::unexpected{ Explain(m_tls.Describe(read.error())) });
	}
}

//////////////////////////////////////////////////////////////////////////
// A kept connection the server has since closed fails before any reply; that request is sent again once, on a new one.
void CFetchPump::Answer(std::expected<std::string, std::string> body)
{
	bool const isAnswered{ body.has_value() };
	bool const isRetried{ !isAnswered && m_isReused && !m_hasReceived && !m_hasRetried };

	if (isRetried)
	{
		m_hasRetried = true;
		CloseConnection();
	}
	else
	{
		m_results.Enqueue(SFetchResult{ std::move(m_requests.front().path), std::move(body) });
		m_requests.pop_front();
		m_hasRetried = false;

		if (!m_isNotified.exchange(true, std::memory_order_acq_rel))
		{
			m_onResults();
		}

		if (isAnswered && m_reader.IsKeptAlive() && !m_requests.empty())
		{
			m_isReused = true;
			m_state = EFetchState::Idle;
		}
		else
		{
			CloseConnection();
		}

		if (m_requests.empty() && m_deadlineTimer.has_value())
		{
			m_loop.CancelTimer(*m_deadlineTimer);
			m_deadlineTimer.reset();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Every request waiting fails for the same reason: none of them could get through either.
void CFetchPump::FailAll(std::string const& reason)
{
	while (!m_requests.empty())
	{
		m_results.Enqueue(SFetchResult{ std::move(m_requests.front().path), std::unexpected{ reason } });
		m_requests.pop_front();
	}

	CloseConnection();

	if (m_deadlineTimer.has_value())
	{
		m_loop.CancelTimer(*m_deadlineTimer);
		m_deadlineTimer.reset();
	}

	if (!m_isNotified.exchange(true, std::memory_order_acq_rel))
	{
		m_onResults();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::CloseConnection()
{
	if (m_watch.has_value())
	{
		m_loop.Unwatch(*m_watch);
		m_watch.reset();
	}

	if (m_pollTimer.has_value())
	{
		m_loop.CancelTimer(*m_pollTimer);
		m_pollTimer.reset();
	}

	m_tls.Terminate();
	m_socket.Terminate();
	m_lookup.Terminate();
	m_isReused = false;
	m_state = EFetchState::Idle;
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::PollIn(std::chrono::milliseconds delay)
{
	if (!m_pollTimer.has_value())
	{
		m_pollTimer = m_loop.ScheduleAt(std::chrono::steady_clock::now() + delay, [this]()
		{
			m_pollTimer.reset();
			Advance();
		});
	}
}

//////////////////////////////////////////////////////////////////////////
// Before a connection stands, the next request could not do better, so all fail; after, only this one does.
void CFetchPump::OnDeadline()
{
	std::string const reason{ Explain(std::format("did not answer within {} s", std::chrono::duration_cast<std::chrono::seconds>(m_origin.timeout).count())) };

	if (m_state == EFetchState::Resolving || m_state == EFetchState::Connecting || m_state == EFetchState::Handshaking)
	{
		FailAll(reason);
	}
	else if (!m_requests.empty())
	{
		m_hasRetried = true;
		Answer(std::unexpected{ reason });
	}
}

//////////////////////////////////////////////////////////////////////////
std::string CFetchPump::Explain(std::string_view reason) const
{
	return std::format("{}: {}", m_origin.host, reason);
}
} // namespace Lkt::Net
