#pragma once

#include <cstdint>

namespace Lkt::Games
{
inline constexpr uint64_t FirstGameFormat{ 1 };
// The newest game.json format this Lookout reads.
inline constexpr uint64_t GameFormat{ 3 };

constexpr bool IsReadableGameFormat(uint64_t format)
{
	return format == FirstGameFormat || format == GameFormat;
}
} // namespace Lkt::Games
