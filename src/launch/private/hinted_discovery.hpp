#pragma once

#include "launch/launch_environment.hpp"
#include "launch/launch_option.hpp"
#include "query/launch_hints.hpp"
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
std::vector<SLaunchOption> FindLaunchOptions(Query::SLaunchHints const& hints, std::string_view gameName, SLaunchEnvironment const& environment);
} // namespace Lkt::Launch
