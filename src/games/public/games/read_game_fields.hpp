#pragma once

#include "games/field_problem.hpp"
#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include <expected>
#include <span>
#include <string_view>

namespace Lkt::Games
{
std::expected<Query::SGameDefinition, SFieldProblem> ReadGameFields(std::string_view text, std::span<Query::SProtocolDefinition const> protocols);
} // namespace Lkt::Games
