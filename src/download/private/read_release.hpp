#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Download
{
std::expected<std::string, std::string> ReadReleaseVersion(std::string_view text);
} // namespace Lkt::Download
