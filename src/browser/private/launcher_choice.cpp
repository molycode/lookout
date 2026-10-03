#include "launcher_choice.hpp"
#include <algorithm>

namespace Lkt::Browser
{
//////////////////////////////////////////////////////////////////////////
std::expected<Launch::SLaunchOption, Launch::ELaunchError> ChooseLauncher(std::span<Launch::SLaunchOption const> discovered,
	std::span<SInstallLauncher const> installs, std::string_view launcherId)
{
	std::expected<Launch::SLaunchOption, Launch::ELaunchError> result{ std::unexpected{ Launch::ELaunchError::NoLauncher } };
	auto const install{ std::ranges::find(installs, launcherId, &SInstallLauncher::id) };

	if (launcherId.empty())
	{
		auto const usable{ std::ranges::find_if(installs, [](SInstallLauncher const& candidate) { return candidate.option.has_value(); }) };

		if (!discovered.empty())
		{
			result = discovered.front();
		}
		else if (usable != installs.end())
		{
			result = usable->option;
		}
	}
	else if (install != installs.end())
	{
		result = install->option;
	}
	else
	{
		auto const option{ std::ranges::find(discovered, launcherId, &Launch::SLaunchOption::id) };

		result = (option != discovered.end())
			? std::expected<Launch::SLaunchOption, Launch::ELaunchError>{ *option }
			: std::unexpected{ Launch::ELaunchError::ChosenLauncherMissing };
	}

	return result;
}
} // namespace Lkt::Browser
