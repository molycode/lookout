#include "loopback_https_server.hpp"
#include <psa/crypto.h>
#include <gtest/gtest.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <format>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr std::string_view HeaderEnd{ "\r\n\r\n" };
constexpr int PollMs{ 20 };

//////////////////////////////////////////////////////////////////////////
std::string GetTlsFixture(std::string_view name)
{
	return (std::filesystem::path{ LKT_FIXTURES_DIR } / "tls" / name).string();
}

//////////////////////////////////////////////////////////////////////////
int SendBlocking(void* pContext, unsigned char const* pData, size_t size)
{
	ssize_t const sent{ ::send(*static_cast<int const*>(pContext), pData, size, MSG_NOSIGNAL) };

	return (sent >= 0) ? static_cast<int>(sent) : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

//////////////////////////////////////////////////////////////////////////
int ReceiveBlocking(void* pContext, unsigned char* pBuffer, size_t size)
{
	ssize_t const received{ ::recv(*static_cast<int const*>(pContext), pBuffer, size, 0) };

	return (received >= 0) ? static_cast<int>(received) : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CLoopbackHttpsServer::Start()
{
	mbedtls_ssl_config_init(&m_config);
	mbedtls_x509_crt_init(&m_certificate);
	mbedtls_pk_init(&m_key);

	bool const isLoaded{ psa_crypto_init() == PSA_SUCCESS
		&& mbedtls_x509_crt_parse_file(&m_certificate, GetTlsFixture("server.pem").c_str()) == 0
		&& mbedtls_pk_parse_keyfile(&m_key, GetTlsFixture("server-key.pem").c_str(), nullptr) == 0
		&& mbedtls_ssl_config_defaults(&m_config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0
		&& mbedtls_ssl_conf_own_cert(&m_config, &m_certificate, &m_key) == 0 };

	sockaddr_in address{};
	socklen_t size{ sizeof(address) };

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	m_listener = isLoaded ? ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0) : -1;

	bool const isListening{ m_listener >= 0 && ::bind(m_listener, reinterpret_cast<sockaddr const*>(&address), sizeof(address)) == 0
		&& ::listen(m_listener, 8) == 0 && ::getsockname(m_listener, reinterpret_cast<sockaddr*>(&address), &size) == 0 };

	EXPECT_TRUE(isLoaded) << "the loopback HTTPS server cannot load tests/fixtures/tls";
	EXPECT_TRUE(isListening) << "the loopback HTTPS server cannot listen";

	if (isListening)
	{
		m_port = ntohs(address.sin_port);
		m_thread = std::thread{ [this]() { Serve(); } };
	}

	return isListening;
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackHttpsServer::Stop()
{
	m_isStopping = true;

	if (m_thread.joinable())
	{
		m_thread.join();
	}

	if (m_listener >= 0)
	{
		::close(m_listener);
		m_listener = -1;
	}

	mbedtls_ssl_config_free(&m_config);
	mbedtls_x509_crt_free(&m_certificate);
	mbedtls_pk_free(&m_key);
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackHttpsServer::SetReply(std::string_view path, SHttpsReply reply)
{
	std::lock_guard const lock{ m_mutex };

	m_replies.insert_or_assign(std::string{ path }, std::move(reply));
}

//////////////////////////////////////////////////////////////////////////
uint16_t CLoopbackHttpsServer::GetPort() const
{
	return m_port;
}

//////////////////////////////////////////////////////////////////////////
size_t CLoopbackHttpsServer::GetNumConnections() const
{
	return m_numConnections;
}

//////////////////////////////////////////////////////////////////////////
size_t CLoopbackHttpsServer::GetNumRequests() const
{
	return m_numRequests;
}

//////////////////////////////////////////////////////////////////////////
// Polled, so Stop is seen within a poll interval wherever the server waits.
void CLoopbackHttpsServer::Serve()
{
	while (!m_isStopping)
	{
		pollfd listener{ m_listener, POLLIN, 0 };

		if (::poll(&listener, 1, PollMs) > 0)
		{
			int const descriptor{ ::accept4(m_listener, nullptr, nullptr, SOCK_CLOEXEC) };

			if (descriptor >= 0)
			{
				++m_numConnections;
				ServeConnection(descriptor);
				::close(descriptor);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackHttpsServer::ServeConnection(int descriptor)
{
	mbedtls_ssl_context ssl{};
	int peer{ descriptor };
	bool isOpen{ true };

	mbedtls_ssl_init(&ssl);
	isOpen = mbedtls_ssl_setup(&ssl, &m_config) == 0;
	mbedtls_ssl_set_bio(&ssl, &peer, SendBlocking, ReceiveBlocking, nullptr);
	isOpen = isOpen && mbedtls_ssl_handshake(&ssl) == 0;

	std::string received{};
	std::array<unsigned char, 4096> buffer{};

	while (isOpen && !m_isStopping)
	{
		size_t const end{ received.find(HeaderEnd) };

		if (end == std::string::npos)
		{
			pollfd connection{ descriptor, POLLIN, 0 };
			bool const isReadable{ mbedtls_ssl_get_bytes_avail(&ssl) > 0 || ::poll(&connection, 1, PollMs) > 0 };
			int const read{ isReadable ? mbedtls_ssl_read(&ssl, buffer.data(), buffer.size()) : MBEDTLS_ERR_SSL_WANT_READ };

			isOpen = read > 0 || read == MBEDTLS_ERR_SSL_WANT_READ;
			received.append(reinterpret_cast<char const*>(buffer.data()), static_cast<size_t>(std::max(read, 0)));
		}
		else
		{
			std::string_view const requestLine{ std::string_view{ received }.substr(0, received.find("\r\n")) };
			size_t const pathStart{ requestLine.find(' ') + 1 };
			std::string_view const path{ requestLine.substr(pathStart, requestLine.find(' ', pathStart) - pathStart) };
			SHttpsReply const reply{ FindReply(path) };
			bool const hasEncoding{ std::ranges::any_of(reply.headers, [](std::string const& header) { return header.starts_with("Transfer-Encoding"); }) };
			std::string response{ std::format("HTTP/1.1 {}\r\n", reply.status) };

			++m_numRequests;
			received.erase(0, end + HeaderEnd.size());

			for (std::string const& header : reply.headers)
			{
				response += std::format("{}\r\n", header);
			}

			response += hasEncoding ? std::format("\r\n{:x}\r\n{}\r\n0\r\n\r\n", reply.body.size(), reply.body)
				: std::format("Content-Length: {}\r\n\r\n{}", reply.body.size(), reply.body);

			while (reply.isSilent && !m_isStopping)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds{ PollMs });
			}

			isOpen = !reply.isSilent && mbedtls_ssl_write(&ssl, reinterpret_cast<unsigned char const*>(response.data()), response.size())
				== static_cast<int>(response.size()) && !reply.dropsConnection;
		}
	}

	mbedtls_ssl_free(&ssl);
}

//////////////////////////////////////////////////////////////////////////
SHttpsReply CLoopbackHttpsServer::FindReply(std::string_view path) const
{
	std::lock_guard const lock{ m_mutex };
	auto const it{ m_replies.find(path) };

	return (it != m_replies.end()) ? it->second : SHttpsReply{ "404 Not Found", "not here", {}, false, false };
}
} // namespace Lkt::Fixtures
