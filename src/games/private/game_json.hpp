#pragma once

#include "query/game_definition.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Games
{
// The error names where the file first went wrong ("masters[0].port: …"); the key is left for the caller, as it is
// the name of the game's folder.
std::expected<Query::SGameDefinition, std::string> ReadGameJson(std::string_view text);
} // namespace Lkt::Games
