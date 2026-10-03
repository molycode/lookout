#pragma once

#include "config/game_settings.hpp"
#include "config/window_settings.hpp"
#include "query/game.hpp"
#include <array>
#include <cstdint>

namespace Lkt::Config
{
struct SSettings final
{
	SWindowSettings window;
	Query::EGame selectedGame{ Query::EGame::Kingpin };
	std::array<SGameSettings, Query::NumGames> games;
	uint32_t autoRefreshSeconds{ 120 };

	bool operator==(SSettings const&) const = default;
};
} // namespace Lkt::Config
