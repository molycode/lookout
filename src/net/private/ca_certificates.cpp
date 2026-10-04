#include "ca_certificates.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>
#include <string>

namespace Lkt::Net
{
namespace
{
// Go's crypto/x509 list, in its order: Debian, Ubuntu, Arch, Gentoo and Mint first, then the Red Hat family and SUSE.
constexpr std::array<char const*, 6> BundleFiles{
	"/etc/ssl/certs/ca-certificates.crt",
	"/etc/pki/tls/certs/ca-bundle.crt",
	"/etc/ssl/ca-bundle.pem",
	"/etc/pki/tls/cacert.pem",
	"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
	"/etc/ssl/cert.pem"
};
constexpr std::array<char const*, 2> BundleFolders{ "/etc/ssl/certs", "/etc/pki/tls/certs" };

//////////////////////////////////////////////////////////////////////////
// A bundle with a few certificates Mbed TLS cannot read still serves: a negative result means none were read.
bool LoadFile(mbedtls_x509_crt& chain, char const* pPath)
{
	return mbedtls_x509_crt_parse_file(&chain, pPath) >= 0 && chain.raw.len != 0;
}

//////////////////////////////////////////////////////////////////////////
bool LoadFolder(mbedtls_x509_crt& chain, char const* pPath)
{
	return mbedtls_x509_crt_parse_path(&chain, pPath) >= 0 && chain.raw.len != 0;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> LoadCaCertificates(mbedtls_x509_crt& chain, std::string_view file)
{
	std::string const override{ file };
	char const* const pEnvironmentFile{ std::getenv("SSL_CERT_FILE") };
	char const* const pEnvironmentFolder{ std::getenv("SSL_CERT_DIR") };
	std::expected<void, std::string> result{};

	if (!override.empty())
	{
		result = LoadFile(chain, override.c_str()) ? std::expected<void, std::string>{}
			: std::unexpected{ std::format("cannot read the CA certificates in {}", override) };
	}
	else if (pEnvironmentFile != nullptr || pEnvironmentFolder != nullptr)
	{
		bool const isLoaded{ (pEnvironmentFile != nullptr && LoadFile(chain, pEnvironmentFile))
			|| (pEnvironmentFolder != nullptr && LoadFolder(chain, pEnvironmentFolder)) };

		result = isLoaded ? std::expected<void, std::string>{}
			: std::unexpected{ std::string{ "cannot read the CA certificates SSL_CERT_FILE or SSL_CERT_DIR names" } };
	}
	else
	{
		bool const isLoaded{ std::ranges::any_of(BundleFiles, [&chain](char const* pPath) { return LoadFile(chain, pPath); })
			|| std::ranges::any_of(BundleFolders, [&chain](char const* pPath) { return LoadFolder(chain, pPath); }) };

		result = isLoaded ? std::expected<void, std::string>{}
			: std::unexpected{ std::string{ "finds no CA certificates on this system; installing the ca-certificates package provides them" } };
	}

	return result;
}
} // namespace Lkt::Net
