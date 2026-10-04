#pragma once

#include <cstdint>
#include <limits>

namespace Lkt::Query
{
// A protocol's position in the catalog, numbered when Lookout starts.
enum class EProtocol : uint8_t
{
};

inline constexpr EProtocol NoProtocol{ std::numeric_limits<uint8_t>::max() };
} // namespace Lkt::Query
