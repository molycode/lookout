#pragma once

#include "games/builtins.hpp"

namespace Lkt::Games
{
// The protocols and games compiled into Lookout, games in sidebar order; one that cannot be read is left out, and
// its problem returned.
SBuiltins LoadBuiltins();
} // namespace Lkt::Games
