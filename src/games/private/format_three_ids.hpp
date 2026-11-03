#pragma once

#include "format_three_id.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace Lkt::Games
{
std::span<SFormatThreeId const> GetFormatThreeIds();
SFormatThreeId const* FindFormatThreeId(std::string_view store);
std::optional<uint64_t> ReadCanonicalDecimal(std::string_view text);
} // namespace Lkt::Games
