#pragma once

#include "games/game_content.hpp"
#include "query/protocol_definition.hpp"
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace Lkt::Games
{
// The downloaded protocols and games with userDir's protocols/ and games/ over them, games by name. Nothing is read from
// an empty path and nothing is created. One that cannot be used is left out, or its downloaded version kept, and its
// problem returned.
SGameContent LoadGames(std::filesystem::path const& downloadedDir, std::filesystem::path const& userDir);
std::expected<Query::SProtocolDefinition, std::string> LoadDownloadedProtocol(std::filesystem::path const& downloadedDir, std::string_view name);
} // namespace Lkt::Games
