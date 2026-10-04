#pragma once

#include "query/game.hpp"
#include "query/game_definition.hpp"
#include <span>
#include <string_view>

namespace Lkt::Query
{
// Filled before any thread starts and read-only until terminated; each game's id is its position.
void InitializeGameCatalog(std::span<SGameDefinition const> games);
void TerminateGameCatalog();

std::span<SGameDefinition const> GetGameCatalog();
SGameDefinition const& GetGame(EGame game);

// Null for a key no game has.
SGameDefinition const* FindGame(std::string_view key);
} // namespace Lkt::Query
