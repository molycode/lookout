#pragma once

#include "browser/install_launcher.hpp"
#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include <expected>
#include <vector>

namespace Lkt::Browser
{
struct SLaunchState final
{
	std::vector<Launch::SLaunchOption> options;
	std::vector<SInstallLauncher> installs;
	std::expected<Launch::SLaunchOption, Launch::ELaunchError> joinLauncher{ std::unexpected{ Launch::ELaunchError::NoLauncher } };
};
} // namespace Lkt::Browser
