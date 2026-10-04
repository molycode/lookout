#pragma once

#include "query/game_definition.hpp"
#include <vector>

namespace Lkt::Query
{
// The games compiled into Lookout, in sidebar order; their ids are given by InitializeGameCatalog.
std::vector<SGameDefinition> GetBuiltinGames();
} // namespace Lkt::Query
