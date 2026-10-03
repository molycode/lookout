#pragma once

#include "launch_hints.hpp"
#include "launch/launch_environment.hpp"
#include "launch/launch_option.hpp"
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
std::vector<SLaunchOption> FindLaunchOptions(SLaunchHints const& hints, std::string_view gameName, SLaunchEnvironment const& environment);
} // namespace Lkt::Launch
