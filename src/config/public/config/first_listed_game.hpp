#pragma once

#include "config/settings.hpp"
#include "query/game.hpp"
#include <optional>

namespace Lkt::Config
{
std::optional<Query::EGame> FindFirstListedGame(SSettings const& settings);
} // namespace Lkt::Config
