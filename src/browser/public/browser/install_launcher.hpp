#pragma once

#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include <expected>
#include <string>

namespace Lkt::Browser
{
struct SInstallLauncher final
{
	std::string id;
	std::string name;
	std::string location;
	std::expected<Launch::SLaunchOption, Launch::ELaunchError> option{ std::unexpected{ Launch::ELaunchError::NoLauncher } };
};
} // namespace Lkt::Browser
