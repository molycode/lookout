#pragma once

#include <expected>
#include <memory>
#include <span>
#include <string>

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
// Each kind of conversation is started once with the game's options, so a game the script cannot talk for fails here.
std::expected<void, std::string> TryConversations(Query::SGameDefinition const& game, std::span<std::unique_ptr<Script::CProtocolScript> const> scripts);
} // namespace Games
} // namespace Lkt
