#include "loopback_https_server.hpp"
#include <psa/crypto.h>
#include <gtest/gtest.h>
#include <tge/net/ipv4_endpoint.hpp>
#include <tge/net/stream_socket.hpp>
#include <tge/net/wait_for_readable.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <span>
#include <system_error>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
constexpr std::string_view HeaderEnd{ "\r\n\r\n" };
constexpr uint32_t Loopback{ 0x7F000001 };
constexpr std::chrono::milliseconds PollInterval{ 20 };

//////////////////////////////////////////////////////////////////////////
std::string GetTlsFixture(std::string_view name)
{
	return (std::filesystem::path{ LKT_FIXTURES_DIR } / "tls" / name).string();
}

//////////////////////////////////////////////////////////////////////////
int SendBlocking(void* pContext, unsigned char const* pData, size_t size)
{
	std::expected<size_t, std::error_code> const sent{ static_cast<Tge::Net::CStreamSocket const*>(pContext)->Send(
		std::span{ reinterpret_cast<std::byte const*>(pData), size }) };

	return sent.has_value() ? static_cast<int>(*sent) : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

//////////////////////////////////////////////////////////////////////////
int ReceiveBlocking(void* pContext, unsigned char* pBuffer, size_t size)
{
	std::expected<size_t, std::error_code> const received{ static_cast<Tge::Net::CStreamSocket const*>(pContext)->Receive(
		std::span{ reinterpret_cast<std::byte*>(pBuffer), size }) };

	return received.has_value() ? static_cast<int>(*received) : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
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

	std::expected<Tge::Net::SIpv4Endpoint, std::error_code> const local{ m_listener.Initialize(Tge::Net::SIpv4Endpoint{ Loopback, 0 })
		.and_then([this]() { return m_listener.GetLocalEndpoint(); }) };
	bool const isListening{ isLoaded && local.has_value() };

	EXPECT_TRUE(isLoaded) << "the loopback HTTPS server cannot load tests/fixtures/tls";
	EXPECT_TRUE(local.has_value()) << "the loopback HTTPS server cannot listen";

	if (isListening)
	{
		m_port = local->port;
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

	m_listener.Terminate();
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
		Tge::Net::CStreamSocket connection{};

		if (Tge::Net::WaitForReadable(m_listener.GetWaitable(), PollInterval).value_or(false) && m_listener.Accept(connection).value_or(false)
			&& connection.SetBlocking().has_value())
		{
			++m_numConnections;
			ServeConnection(connection);
		}

		connection.Terminate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CLoopbackHttpsServer::ServeConnection(Tge::Net::CStreamSocket& connection)
{
	mbedtls_ssl_context ssl{};
	bool isOpen{ true };

	mbedtls_ssl_init(&ssl);
	isOpen = mbedtls_ssl_setup(&ssl, &m_config) == 0;
	mbedtls_ssl_set_bio(&ssl, &connection, SendBlocking, ReceiveBlocking, nullptr);
	isOpen = isOpen && mbedtls_ssl_handshake(&ssl) == 0;

	std::string received{};
	std::array<unsigned char, 4096> buffer{};

	while (isOpen && !m_isStopping)
	{
		size_t const end{ received.find(HeaderEnd) };

		if (end == std::string::npos)
		{
			bool const isReadable{ mbedtls_ssl_get_bytes_avail(&ssl) > 0 || Tge::Net::WaitForReadable(connection.GetWaitable(), PollInterval).value_or(false) };
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
				std::this_thread::sleep_for(PollInterval);
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
