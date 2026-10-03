#pragma once

#include <cstddef>
#include <cstdint>

namespace Lkt::Config
{
enum class ESortColumn : uint8_t
{
	Name,
	Map,
	Mod,
	Mode,
	Players,
	Ping,
	Favourite,
	Password
};

inline constexpr size_t NumSortColumns{ static_cast<size_t>(ESortColumn::Password) + 1 };
} // namespace Lkt::Config
