#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Lkt::Download
{
std::optional<std::array<uint32_t, 3>> ParseLookoutVersion(std::string_view text);
} // namespace Lkt::Download
