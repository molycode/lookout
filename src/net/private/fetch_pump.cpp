#include "fetch_pump.hpp"
#include "ca_certificates.hpp"
#include "loggers.hpp"
#include <psa/crypto.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <format>
#include <span>
#include <string_view>
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
// The first IPv4 address, with the origin's port; glibc's results are freed either way.
std::optional<Query::SServerAddress> TakeAddress(gaicb& request, uint16_t port)
{
	std::optional<Query::SServerAddress> address{};

	for (addrinfo const* pInfo{ request.ar_result }; pInfo != nullptr && !address.has_value(); pInfo = pInfo->ai_next)
	{
		if (pInfo->ai_family == AF_INET)
		{
			sockaddr_in const* const pIpv4{ reinterpret_cast<sockaddr_in const*>(pInfo->ai_addr) };

			address = Query::SServerAddress{ ntohl(pIpv4->sin_addr.s_addr), port };
		}
	}

	if (request.ar_result != nullptr)
	{
		freeaddrinfo(request.ar_result);
		request.ar_result = nullptr;
	}

	return address;
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
	AbandonLookup();
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
// Polled, not notified: a notification would run on a glibc thread and race the loop.
void CFetchPump::Resolve()
{
	m_pLookup = std::make_unique<SHostLookup>();
	m_pLookup->host = m_origin.host;
	m_pLookup->hints.ai_family = AF_INET;
	m_pLookup->hints.ai_socktype = SOCK_STREAM;
	m_pLookup->request.ar_name = m_pLookup->host.c_str();
	m_pLookup->request.ar_request = &m_pLookup->hints;

	std::array<gaicb*, 1> requests{ &m_pLookup->request };
	int const status{ getaddrinfo_a(GAI_NOWAIT, requests.data(), static_cast<int>(requests.size()), nullptr) };

	if (status == 0)
	{
		m_state = EFetchState::Resolving;
	}
	else
	{
		m_pLookup.reset();
		FailAll(Explain(std::format("cannot be looked up: {}", gai_strerror(status))));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::CollectLookup()
{
	int const status{ gai_error(&m_pLookup->request) };

	if (status == EAI_INPROGRESS)
	{
		PollIn(LookupPoll);
	}
	else
	{
		std::optional<Query::SServerAddress> const address{ TakeAddress(m_pLookup->request, m_origin.port) };

		m_pLookup.reset();

		if (address.has_value())
		{
			Connect(*address);
		}
		else
		{
			FailAll(Explain(std::format("cannot be looked up: {}", (status == 0) ? "it has no IPv4 address" : gai_strerror(status))));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::Connect(Query::SServerAddress const& address)
{
	std::expected<void, int> const connecting{ m_socket.Initialize() ? m_socket.Connect(address) : std::unexpected{ errno } };

	if (connecting.has_value())
	{
		m_state = EFetchState::Connecting;
	}
	else
	{
		FailAll(Explain(std::format("cannot be reached: {}", std::strerror(connecting.error()))));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFetchPump::CheckConnected()
{
	pollfd descriptor{ m_socket.GetDescriptor(), POLLOUT, 0 };
	int error{ 0 };
	socklen_t size{ sizeof(error) };
	bool const isSettled{ ::poll(&descriptor, 1, 0) > 0 };

	if (isSettled && getsockopt(m_socket.GetDescriptor(), SOL_SOCKET, SO_ERROR, &error, &size) != 0)
	{
		error = errno;
	}

	if (!isSettled)
	{
		PollIn(SocketPoll);
	}
	else if (error != 0)
	{
		FailAll(Explain(std::format("cannot be reached: {}", std::strerror(error))));
	}
	else
	{
		std::expected<void, std::string> const secured{ m_tls.Initialize(m_config, m_origin.host, m_socket.GetDescriptor()) };

		m_watch = secured.has_value() ? m_loop.Watch(m_socket.GetDescriptor(), [this]() { Advance(); }) : std::nullopt;

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
	AbandonLookup();
	m_isReused = false;
	m_state = EFetchState::Idle;
}

//////////////////////////////////////////////////////////////////////////
// A lookup glibc cannot cancel still writes into its request, so that request is left to glibc, never freed.
void CFetchPump::AbandonLookup()
{
	if (m_pLookup != nullptr && gai_cancel(&m_pLookup->request) == EAI_NOTCANCELED)
	{
		static_cast<void>(m_pLookup.release());
	}
	else if (m_pLookup != nullptr && m_pLookup->request.ar_result != nullptr)
	{
		freeaddrinfo(m_pLookup->request.ar_result);
	}

	m_pLookup.reset();
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
