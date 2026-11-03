#pragma once

#include "launch/found_store.hpp"
#include "launch/launch_environment.hpp"
#include "launch/launch_option.hpp"
#include "query/launch_hints.hpp"
#include <span>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
std::vector<SLaunchOption> FindLaunchOptions(Query::SLaunchHints const& hints, std::string_view gameName, SLaunchEnvironment const& environment,
	std::span<SFoundStore const> stores);
} // namespace Lkt::Launch
