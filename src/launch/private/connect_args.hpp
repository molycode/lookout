#pragma once

#include "launch/connect_request.hpp"
#include "launch/launch_error.hpp"
#include "query/join_command.hpp"
#include <expected>
#include <string>
#include <vector>

namespace Lkt::Launch
{
// While the game runs, its password stays readable in /proc/<pid>/cmdline: a command line is all a launcher can pass.
std::expected<std::vector<std::string>, ELaunchError> BuildConnectArgs(Query::SJoinCommand const& join, SConnectRequest const& request);
} // namespace Lkt::Launch
