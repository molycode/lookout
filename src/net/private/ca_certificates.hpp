#pragma once

#include <mbedtls/x509_crt.h>
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Net
{
// The system's trusted CA certificates, from SSL_CERT_FILE or SSL_CERT_DIR when set, else from where the distributions
// keep their bundle. A non-empty file replaces them all; tests trust their own CA that way.
std::expected<void, std::string> LoadCaCertificates(mbedtls_x509_crt& chain, std::string_view file);
} // namespace Lkt::Net
