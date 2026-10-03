#pragma once

#include "browser/install_launcher.hpp"
#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include <expected>
#include <span>
#include <string_view>

namespace Lkt::Browser
{
// An empty id means the first that can start: a discovered launcher, else an install.
std::expected<Launch::SLaunchOption, Launch::ELaunchError> ChooseLauncher(std::span<Launch::SLaunchOption const> discovered,
	std::span<SInstallLauncher const> installs, std::string_view launcherId);
} // namespace Lkt::Browser
