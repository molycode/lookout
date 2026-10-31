#include "ca_certificates.hpp"
#include <tge/net/root_certificates.hpp>
#include <tge/platform.hpp>
#include <cstddef>
#include <format>
#include <string>
#include <system_error>
#include <vector>

namespace Lkt::Net
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::string DescribeMissing(std::error_code error)
{
	std::string description{ std::format("cannot read the system's CA certificates: {}", error.message()) };

	if (error == std::errc::invalid_argument)
	{
		description = "cannot read the CA certificates SSL_CERT_FILE or SSL_CERT_DIR names";
	}
	else if (error == std::errc::no_such_file_or_directory)
	{
#if defined(TGE_PLATFORM_WINDOWS)
		description = "finds no CA certificates in the Windows root store";
#elif defined(TGE_PLATFORM_LINUX)
		description = "finds no CA certificates on this system; installing the ca-certificates package provides them";
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
	}

	return description;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// A non-empty file replaces the system's certificates; tests trust their own CA that way.
std::expected<void, std::string> LoadCaCertificates(mbedtls_x509_crt& chain, std::string_view file)
{
	std::string const override{ file };
	std::expected<void, std::string> result{};

	if (!override.empty())
	{
		result = (mbedtls_x509_crt_parse_file(&chain, override.c_str()) >= 0 && chain.raw.len != 0) ? std::expected<void, std::string>{}
			: std::unexpected{ std::format("cannot read the CA certificates in {}", override) };
	}
	else
	{
		std::expected<std::vector<std::vector<std::byte>>, std::error_code> const certificates{ Tge::Net::ReadSystemRootCertificates() };

		if (certificates.has_value())
		{
			// One certificate Mbed TLS cannot read leaves the others usable.
			for (std::vector<std::byte> const& certificate : *certificates)
			{
				static_cast<void>(mbedtls_x509_crt_parse_der(&chain, reinterpret_cast<unsigned char const*>(certificate.data()), certificate.size()));
			}

			result = (chain.raw.len != 0) ? std::expected<void, std::string>{} : std::unexpected{ std::string{ "cannot read any of the system's CA certificates" } };
		}
		else
		{
			result = std::unexpected{ DescribeMissing(certificates.error()) };
		}
	}

	return result;
}
} // namespace Lkt::Net
