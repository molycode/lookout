#pragma once

#include "query/game.hpp"
#include "query/game_definition.hpp"
#include <span>
#include <string_view>

namespace Lkt::Query
{
std::span<SGameDefinition const> GetGameCatalog();
SGameDefinition const& GetGame(EGame game);

// Null for a key no game has.
SGameDefinition const* FindGame(std::string_view key);
} // namespace Lkt::Query
