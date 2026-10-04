#pragma once

#include "launch/launch_environment.hpp"
#include "launch/launch_option.hpp"
#include <vector>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Launch
{
// Desktop entries first, then the default install folder; each rejected candidate is logged.
std::vector<SLaunchOption> FindLaunchOptions(Query::SGameDefinition const& game, SLaunchEnvironment const& environment);
} // namespace Launch
} // namespace Lkt
