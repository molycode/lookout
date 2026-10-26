#pragma once

#include <string>
#include <string_view>

namespace Lkt::Download
{
std::string MakeUserAgent(std::string_view version);
} // namespace Lkt::Download
