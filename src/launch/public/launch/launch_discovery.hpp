#pragma once

#include "launch/launch_environment.hpp"
#include "launch/launch_option.hpp"
#include "query/game.hpp"
#include <vector>

namespace Lkt::Launch
{
// Desktop entries first, then the default install folder; each rejected candidate is logged.
std::vector<SLaunchOption> FindLaunchOptions(Query::EGame game, SLaunchEnvironment const& environment);
} // namespace Lkt::Launch
