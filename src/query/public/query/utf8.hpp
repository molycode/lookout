#pragma once

#include <string_view>

namespace Lkt::Query
{
bool IsValidUtf8(std::string_view text);
} // namespace Lkt::Query
