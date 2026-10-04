#include "config/first_listed_game.hpp"
#include <algorithm>
#include <cstddef>

namespace Lkt::Config
{
//////////////////////////////////////////////////////////////////////////
std::optional<Query::EGame> FindFirstListedGame(SSettings const& settings)
{
	auto const it{ std::ranges::find_if(settings.gameOrder, [&settings](Query::EGame game)
	{
		return settings.games[static_cast<size_t>(game)].isListed;
	}) };

	return (it != settings.gameOrder.end()) ? std::optional<Query::EGame>{ *it } : std::nullopt;
}
} // namespace Lkt::Config
