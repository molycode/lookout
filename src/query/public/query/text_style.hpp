#pragma once

#include <cstdint>

namespace Lkt::Query
{
// How a game encodes colour in names: Quake 2 engines set the high bit, the Quake 3 family uses ^ codes.
enum class ETextStyle : uint8_t
{
	Ascii7,
	Quake3,
	EnemyTerritory
};
} // namespace Lkt::Query
