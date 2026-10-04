#pragma once

#include "game_move.hpp"
#include "server_action.hpp"
#include "config/server_filter.hpp"
#include "config/sort_order.hpp"
#include "query/game.hpp"
#include <cstdint>
#include <optional>

namespace Lkt::Ui
{
// What the views asked for while drawing. Applied only once every view has drawn, so no span a view holds from the
// browser changes under it mid-frame.
struct SFrameIntents final
{
	std::optional<Query::EGame> selectGame;
	std::optional<Query::EGame> hideGame;
	std::optional<Query::EGame> showGame;
	std::optional<SGameMove> moveGame;
	std::optional<Query::EGame> openGameSettings;
	std::optional<Query::EGame> editGame;
	std::optional<Query::EGame> revertGame;
	std::optional<Query::EGame> removeGame;
	std::optional<Config::SServerFilter> filter;
	std::optional<Config::SSortOrder> sort;
	std::optional<uint32_t> autoRefreshSeconds;
	SServerAction action;
	bool refresh{ false };
	bool openAddServer{ false };
	bool openAddGame{ false };
	bool openAbout{ false };
	bool quit{ false };
};
} // namespace Lkt::Ui
