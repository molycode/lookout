#pragma once

#include "games/field_problem.hpp"
#include "query/protocol_definition.hpp"
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Games
{
// As the loader would take the text: read, then each conversation started once with its protocol.
std::expected<void, SFieldProblem> CheckGameText(std::string_view text, std::span<Query::SProtocolDefinition const> protocols);
} // namespace Lkt::Games
