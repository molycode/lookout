#pragma once

#include <cstdint>
#include <limits>

namespace Lkt::Query
{
// A game's position in the catalog, numbered when Lookout starts.
enum class EGame : uint16_t
{
};

inline constexpr EGame NoGame{ std::numeric_limits<uint16_t>::max() };
} // namespace Lkt::Query
