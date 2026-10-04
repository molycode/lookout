#pragma once

#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include <string>
#include <vector>

namespace Lkt::Games
{
// Problems are reported by the caller: loading runs before logging reaches its file.
struct SBuiltins final
{
	std::vector<Query::SProtocolDefinition> protocols;
	std::vector<Query::SGameDefinition> games;
	std::vector<std::string> problems;
};
} // namespace Lkt::Games
