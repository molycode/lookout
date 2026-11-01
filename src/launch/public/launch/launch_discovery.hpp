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
std::vector<SLaunchOption> FindLaunchOptions(Query::SGameDefinition const& game, SLaunchEnvironment const& environment);
} // namespace Launch
} // namespace Lkt
