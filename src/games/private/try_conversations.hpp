#pragma once

#include "games/field_problem.hpp"
#include <expected>
#include <memory>
#include <span>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Script
{
class CProtocolScript;
} // namespace Script

namespace Games
{
// Each kind of conversation is started once with the game's options, and each master's own list conversation, so a
// game the scripts cannot talk for fails here.
std::expected<void, SFieldProblem> TryConversations(Query::SGameDefinition const& game, std::span<std::unique_ptr<Script::CProtocolScript> const> scripts);
} // namespace Games
} // namespace Lkt
