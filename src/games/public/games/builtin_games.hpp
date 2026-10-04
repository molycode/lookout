#pragma once

#include "query/game_definition.hpp"
#include <vector>

namespace Lkt::Games
{
// In sidebar order; a built-in game that cannot be read is logged and left out.
std::vector<Query::SGameDefinition> LoadBuiltinGames();
} // namespace Lkt::Games
