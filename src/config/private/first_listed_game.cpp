#include "config/first_listed_game.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <algorithm>
#include <span>

namespace Lkt::Config
{
//////////////////////////////////////////////////////////////////////////
std::optional<Query::EGame> FindFirstListedGame(SSettings const& settings)
{
	std::span<Query::SGameDefinition const> const catalog{ Query::GetGameCatalog() };
	auto const it{ std::ranges::find_if(catalog, [&settings](Query::SGameDefinition const& game)
	{
		return settings.games[static_cast<size_t>(game.game)].isListed;
	}) };

	return (it != catalog.end()) ? std::optional<Query::EGame>{ it->game } : std::nullopt;
}
} // namespace Lkt::Config
