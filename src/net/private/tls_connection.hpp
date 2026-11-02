#pragma once

#include <mbedtls/ssl.h>
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace Tge::Net
{
class CStreamSocket;
} // namespace Tge::Net

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
	std::expected<void, std::string> Initialize(mbedtls_ssl_config const& config, std::string_view host, Tge::Net::CStreamSocket const& socket,
		bool hasSystemRoots);
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
	static int Verify(void* pContext, mbedtls_x509_crt* pCertificate, int depth, uint32_t* pFlags);

	bool IsTrustedBySystem(mbedtls_x509_crt const* pServer);

	mbedtls_ssl_context m_ssl{};
	Tge::Net::CStreamSocket const* m_pSocket{ nullptr };
	std::string m_hostName{};
	std::error_code m_systemVerdict{};
	bool m_hasSeenTop{ false };
	bool m_isRootLeftToSystem{ false };
	bool m_hasOtherFlaw{ false };
	bool m_isInitialized{ false };
};
} // namespace Lkt::Net
