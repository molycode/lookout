#include "config/default_settings.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <tge/assert.hpp>
#include <span>

namespace Lkt::Config
{
//////////////////////////////////////////////////////////////////////////
SSettings MakeDefaultSettings()
{
	std::span<Query::SGameDefinition const> const catalog{ Query::GetGameCatalog() };
	SSettings settings{};

	TGE_ASSERT(!catalog.empty(), "Settings need the game catalog to be initialized");

	settings.games.assign(catalog.size(), SGameSettings{});
	settings.gameOrder.reserve(catalog.size());

	for (Query::SGameDefinition const& game : catalog)
	{
		settings.gameOrder.emplace_back(game.game);
	}

	settings.selectedGame = catalog.front().game;

	return settings;
}
} // namespace Lkt::Config
