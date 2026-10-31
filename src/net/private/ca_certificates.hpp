#pragma once

#include <mbedtls/x509_crt.h>
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Net
{
std::expected<void, std::string> LoadCaCertificates(mbedtls_x509_crt& chain, std::string_view file);
} // namespace Lkt::Net
