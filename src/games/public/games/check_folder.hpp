#pragma once

#include "games/game_content.hpp"
#include <filesystem>

namespace Lkt::Games
{
// What lookout-games asks of a folder before it is published: everything LoadGames would refuse, an icon that is not a
// square PNG of at least 128 px or has no licence beside it, and any other file in a game's folder.
SGameContent CheckGameFolder(std::filesystem::path const& folder);
} // namespace Lkt::Games
