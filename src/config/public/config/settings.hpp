#pragma once

#include "config/game_settings.hpp"
#include "config/window_settings.hpp"
#include "query/game.hpp"
#include <cstdint>
#include <vector>

namespace Lkt::Config
{
struct SSettings final
{
	SWindowSettings window;
	Query::EGame selectedGame{ Query::NoGame };
	std::vector<SGameSettings> games;
	std::vector<Query::EGame> gameOrder;
	uint32_t autoRefreshSeconds{ 120 };

	bool operator==(SSettings const&) const = default;
};
} // namespace Lkt::Config
