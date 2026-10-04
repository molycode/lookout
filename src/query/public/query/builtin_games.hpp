#pragma once

#include "query/game_definition.hpp"
#include <span>

namespace Lkt::Query
{
// The games compiled into Lookout, in sidebar order; their ids are given by InitializeGameCatalog.
std::span<SGameDefinition const> GetBuiltinGames();
} // namespace Lkt::Query
