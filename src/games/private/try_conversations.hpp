#pragma once

#include <expected>
#include <map>
#include <string>

namespace Lkt
{
namespace Script
{
class CProtocolScript;
} // namespace Script

namespace Games
{
// Each kind of conversation is started once with the game's options, so a game the script cannot talk for fails here.
std::expected<void, std::string> TryConversations(Script::CProtocolScript& script, std::map<std::string, std::string> const& options);
} // namespace Games
} // namespace Lkt
