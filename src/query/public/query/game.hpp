#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace Lkt::Query
{
enum class EGame : uint16_t
{
	Kingpin,
	Quake2,
	RtcwMultiplayer,
	EnemyTerritory,
	Quake3
};

inline constexpr size_t NumGames{ static_cast<size_t>(EGame::Quake3) + 1 };
inline constexpr EGame NoGame{ std::numeric_limits<uint16_t>::max() };
} // namespace Lkt::Query
