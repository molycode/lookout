#pragma once

#include <cstddef>
#include <cstdint>

namespace Lkt::Query
{
enum class EGame : uint8_t
{
	Kingpin,
	Quake2,
	RtcwMultiplayer,
	EnemyTerritory,
	Quake3
};

inline constexpr size_t NumGames{ static_cast<size_t>(EGame::Quake3) + 1 };
} // namespace Lkt::Query
