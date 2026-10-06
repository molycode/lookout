#include "query/game_catalog.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <cstddef>
#include <vector>

namespace Lkt::Query
{
namespace
{
constinit std::vector<SProtocolDefinition> gProtocols{};
constinit std::vector<SGameDefinition> gGames{};
} // namespace

//////////////////////////////////////////////////////////////////////////
void InitializeGameCatalog(std::span<SProtocolDefinition const> protocols, std::span<SGameDefinition const> games)
{
	TGE_ASSERT(gGames.empty() && gProtocols.empty(), "The game catalog is already initialized");
	TGE_ASSERT(games.size() < static_cast<size_t>(NoGame), "More games than EGame can number");
	TGE_ASSERT(protocols.size() < static_cast<size_t>(NoProtocol), "More protocols than EProtocol can number");
	TGE_ASSERT(std::ranges::all_of(games, [&protocols](SGameDefinition const& game) { return static_cast<size_t>(game.protocol) < protocols.size(); }),
		"A game names a protocol the catalog does not have");

	gProtocols.assign(protocols.begin(), protocols.end());
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
	gProtocols.clear();
	gProtocols.shrink_to_fit();
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

//////////////////////////////////////////////////////////////////////////
std::span<SProtocolDefinition const> GetProtocolCatalog()
{
	return gProtocols;
}

//////////////////////////////////////////////////////////////////////////
SProtocolDefinition const& GetProtocol(EProtocol protocol)
{
	size_t const index{ static_cast<size_t>(protocol) };

	TGE_ASSERT(index < gProtocols.size(), "EProtocol value outside the catalog");

	return gProtocols[index];
}

//////////////////////////////////////////////////////////////////////////
SProtocolDefinition const* FindProtocol(std::string_view name)
{
	auto const it{ std::ranges::find(gProtocols, name, &SProtocolDefinition::name) };

	return (it != gProtocols.end()) ? &*it : nullptr;
}
} // namespace Lkt::Query
