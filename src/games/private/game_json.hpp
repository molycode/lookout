#pragma once

#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Games
{
// The error names where the file first went wrong ("masters[0].port: …"). The key is left for the caller: it is the
// name of the game's folder.
std::expected<Query::SGameDefinition, std::string> ReadGameJson(std::string_view text, std::span<Query::SProtocolDefinition const> protocols);
} // namespace Lkt::Games
