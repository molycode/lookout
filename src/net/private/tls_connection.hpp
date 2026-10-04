#pragma once

#include <mbedtls/ssl.h>
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Net
{
// TLS over a connected non-blocking socket the caller owns. Every call returns at once: an error equal to
// MBEDTLS_ERR_SSL_WANT_READ or MBEDTLS_ERR_SSL_WANT_WRITE means try again once the socket is ready.
class CTlsConnection final : private Tge::SNoCopyNoMove
{
public:

	CTlsConnection() = default;
	~CTlsConnection() = default;

	// config must outlive the connection; host is the name the certificate must carry.
	std::expected<void, std::string> Initialize(mbedtls_ssl_config const& config, std::string_view host, int descriptor);
	void Terminate();

	std::expected<void, int> Handshake();
	std::expected<size_t, int> Write(std::span<std::byte const> data);
	// Zero bytes when the peer closed the connection.
	std::expected<size_t, int> Read(std::span<std::byte> buffer);

	// What a failed call's error means, with the certificate's flaws when it was not trusted.
	std::string Describe(int error) const;

private:

	static int Send(void* pContext, unsigned char const* pData, size_t size);
	static int Receive(void* pContext, unsigned char* pBuffer, size_t size);

	mbedtls_ssl_context m_ssl{};
	int m_descriptor{ -1 };
	bool m_isInitialized{ false };
};
} // namespace Lkt::Net
