#include "config/default_settings.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <span>

namespace Lkt::Config
{
//////////////////////////////////////////////////////////////////////////
SSettings MakeDefaultSettings()
{
	std::span<Query::SGameDefinition const> const catalog{ Query::GetGameCatalog() };
	SSettings settings{};

	settings.games.assign(catalog.size(), SGameSettings{});
	settings.gameOrder.reserve(catalog.size());

	for (Query::SGameDefinition const& game : catalog)
	{
		settings.gameOrder.emplace_back(game.game);
	}

	settings.selectedGame = catalog.empty() ? Query::NoGame : catalog.front().game;

	return settings;
}
} // namespace Lkt::Config
