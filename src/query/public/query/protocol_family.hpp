#pragma once

#include <cstddef>
#include <cstdint>

namespace Lkt::Query
{
enum class EProtocolFamily : uint8_t
{
	Quake2,
	Quake3
};

inline constexpr size_t NumProtocolFamilies{ static_cast<size_t>(EProtocolFamily::Quake3) + 1 };
} // namespace Lkt::Query
