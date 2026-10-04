#include "query/game_catalog.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <cstddef>
#include <vector>

namespace Lkt::Query
{
namespace
{
constinit std::vector<SGameDefinition> gGames{};
} // namespace

//////////////////////////////////////////////////////////////////////////
void InitializeGameCatalog(std::span<SGameDefinition const> games)
{
	TGE_ASSERT(gGames.empty(), "The game catalog is already initialized");
	TGE_ASSERT(games.size() < static_cast<size_t>(NoGame), "More games than EGame can number");

	gGames.assign(games.begin(), games.end());

	for (size_t index{ 0 }; index < gGames.size(); ++index)
	{
		TGE_ASSERT(std::ranges::count(gGames, gGames[index].key, &SGameDefinition::key) == 1, "Two games share a key");

		gGames[index].game = static_cast<EGame>(index);
	}
}

//////////////////////////////////////////////////////////////////////////
void TerminateGameCatalog()
{
	gGames.clear();
	gGames.shrink_to_fit();
}

//////////////////////////////////////////////////////////////////////////
std::span<SGameDefinition const> GetGameCatalog()
{
	return gGames;
}

//////////////////////////////////////////////////////////////////////////
SGameDefinition const& GetGame(EGame game)
{
	size_t const index{ static_cast<size_t>(game) };

	TGE_ASSERT(index < gGames.size(), "EGame value outside the catalog");

	return gGames[index];
}

//////////////////////////////////////////////////////////////////////////
SGameDefinition const* FindGame(std::string_view key)
{
	auto const it{ std::ranges::find(gGames, key, &SGameDefinition::key) };

	return (it != gGames.end()) ? &*it : nullptr;
}
} // namespace Lkt::Query
