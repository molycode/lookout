#include "tls_connection.hpp"
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>
#include <tge/net/stream_socket.hpp>
#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <system_error>

namespace Lkt::Net
{
namespace
{
constexpr size_t MaxDescriptionSize{ 512 };

//////////////////////////////////////////////////////////////////////////
bool IsWouldBlock(std::error_code error)
{
	return error == std::errc::operation_would_block || error == std::errc::interrupted;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> CTlsConnection::Initialize(mbedtls_ssl_config const& config, std::string_view host, Tge::Net::CStreamSocket const& socket)
{
	std::string const hostName{ host };
	std::expected<void, std::string> result{};

	mbedtls_ssl_init(&m_ssl);
	m_pSocket = &socket;
	m_isInitialized = true;

	int const setUp{ mbedtls_ssl_setup(&m_ssl, &config) };
	int const named{ (setUp == 0) ? mbedtls_ssl_set_hostname(&m_ssl, hostName.c_str()) : setUp };

	if (named == 0)
	{
		mbedtls_ssl_set_bio(&m_ssl, this, Send, Receive, nullptr);
	}
	else
	{
		result = std::unexpected{ Describe(named) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CTlsConnection::Terminate()
{
	if (m_isInitialized)
	{
		mbedtls_ssl_free(&m_ssl);
		m_isInitialized = false;
	}

	m_pSocket = nullptr;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, int> CTlsConnection::Handshake()
{
	int const result{ mbedtls_ssl_handshake(&m_ssl) };

	return (result == 0) ? std::expected<void, int>{} : std::unexpected{ result };
}

//////////////////////////////////////////////////////////////////////////
std::expected<size_t, int> CTlsConnection::Write(std::span<std::byte const> data)
{
	int const result{ mbedtls_ssl_write(&m_ssl, reinterpret_cast<unsigned char const*>(data.data()), data.size()) };

	return (result >= 0) ? std::expected<size_t, int>{ static_cast<size_t>(result) } : std::unexpected{ result };
}

//////////////////////////////////////////////////////////////////////////
// A peer's close_notify ends the stream as a plain close does.
std::expected<size_t, int> CTlsConnection::Read(std::span<std::byte> buffer)
{
	int const result{ mbedtls_ssl_read(&m_ssl, reinterpret_cast<unsigned char*>(buffer.data()), buffer.size()) };
	std::expected<size_t, int> read{ static_cast<size_t>(0) };

	if (result > 0)
	{
		read = static_cast<size_t>(result);
	}
	else if (result < 0 && result != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
	{
		read = std::unexpected{ result };
	}

	return read;
}

//////////////////////////////////////////////////////////////////////////
std::string CTlsConnection::Describe(int error) const
{
	std::array<char, MaxDescriptionSize> buffer{};
	std::string description{};

	if (error == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
	{
		uint32_t const flags{ mbedtls_ssl_get_verify_result(&m_ssl) };
		int const written{ mbedtls_x509_crt_verify_info(buffer.data(), buffer.size(), "", flags) };
		std::string_view text{ buffer.data(), (written > 0) ? static_cast<size_t>(written) : 0 };

		while (text.ends_with('\n'))
		{
			text.remove_suffix(1);
		}

		description = "its certificate is not trusted: ";

		for (size_t at{ 0 }; at <= text.size();)
		{
			size_t const end{ std::min(text.find('\n', at), text.size()) };

			description += std::format("{}{}", (at == 0) ? "" : "; ", text.substr(at, end - at));
			at = end + 1;
		}
	}
	else
	{
		mbedtls_strerror(error, buffer.data(), buffer.size());
		description = std::format("TLS failed: {}", buffer.data());
	}

	return description;
}

//////////////////////////////////////////////////////////////////////////
int CTlsConnection::Send(void* pContext, unsigned char const* pData, size_t size)
{
	CTlsConnection const* const pConnection{ static_cast<CTlsConnection const*>(pContext) };
	std::expected<size_t, std::error_code> const sent{ pConnection->m_pSocket->Send(std::span{ reinterpret_cast<std::byte const*>(pData), size }) };
	int result{ MBEDTLS_ERR_NET_SEND_FAILED };

	if (sent.has_value())
	{
		result = static_cast<int>(*sent);
	}
	else if (IsWouldBlock(sent.error()))
	{
		result = MBEDTLS_ERR_SSL_WANT_WRITE;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
int CTlsConnection::Receive(void* pContext, unsigned char* pBuffer, size_t size)
{
	CTlsConnection const* const pConnection{ static_cast<CTlsConnection const*>(pContext) };
	std::expected<size_t, std::error_code> const received{ pConnection->m_pSocket->Receive(std::span{ reinterpret_cast<std::byte*>(pBuffer), size }) };
	int result{ MBEDTLS_ERR_NET_RECV_FAILED };

	if (received.has_value())
	{
		result = static_cast<int>(*received);
	}
	else if (IsWouldBlock(received.error()))
	{
		result = MBEDTLS_ERR_SSL_WANT_READ;
	}

	return result;
}
} // namespace Lkt::Net
