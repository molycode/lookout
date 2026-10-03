#pragma once

#include "launch/connect_request.hpp"
#include "launch/launch_error.hpp"
#include <expected>
#include <string>
#include <vector>

namespace Lkt::Launch
{
// While the game runs, its password stays readable in /proc/<pid>/cmdline: the engines take it nowhere else.
std::expected<std::vector<std::string>, ELaunchError> BuildConnectArgs(SConnectRequest const& request);
} // namespace Lkt::Launch
