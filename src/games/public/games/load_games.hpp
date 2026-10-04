#pragma once

#include "games/game_content.hpp"
#include <filesystem>

namespace Lkt::Games
{
// The built-in protocols and games with userDir's protocols/ and games/ over them, games by name; nothing is read from
// an empty userDir and nothing is created. One that cannot be used is left out, or its built-in kept, and its problem
// returned.
SGameContent LoadGames(std::filesystem::path const& userDir);
} // namespace Lkt::Games
