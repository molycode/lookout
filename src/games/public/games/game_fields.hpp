#pragma once

#include "games/game_field.hpp"
#include <span>

namespace Lkt::Games
{
// Every field, each object's own fields right after it.
std::span<SGameField const> GetGameFields();
} // namespace Lkt::Games
