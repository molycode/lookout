#pragma once

#include "query/game_definition.hpp"
#include "query/game_problem.hpp"
#include "query/protocol_definition.hpp"
#include <vector>

namespace Lkt::Games
{
// Problems are reported by the caller: loading runs before logging reaches its file.
struct SGameContent final
{
	std::vector<Query::SProtocolDefinition> protocols;
	std::vector<Query::SGameDefinition> games;
	std::vector<Query::SGameProblem> problems;
};
} // namespace Lkt::Games
