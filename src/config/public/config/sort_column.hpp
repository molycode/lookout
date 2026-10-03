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
	Password,
	Country
};

inline constexpr size_t NumSortColumns{ static_cast<size_t>(ESortColumn::Country) + 1 };
} // namespace Lkt::Config
