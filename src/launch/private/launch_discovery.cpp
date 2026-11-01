#include "launch/launch_discovery.hpp"
#include "desktop_entry_source.hpp"
#include "hinted_discovery.hpp"
#include "home_install_source.hpp"
#include "query/game_definition.hpp"
#include <tge/platform.hpp>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
bool StartsTheSameProgram(SLaunchOption const& lhs, SLaunchOption const& rhs)
{
	std::error_code error{};

	return std::filesystem::equivalent(lhs.argv.front(), rhs.argv.front(), error) && error.value() == 0;
}

//////////////////////////////////////////////////////////////////////////
void AppendDistinct(std::vector<SLaunchOption> found, std::vector<SLaunchOption>& options)
{
	for (SLaunchOption& option : found)
	{
		if (std::ranges::none_of(options, [&option](SLaunchOption const& listed) { return StartsTheSameProgram(listed, option); }))
		{
			options.emplace_back(std::move(option));
		}
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Desktop entries may share a wrapper such as flatpak, so only the install folder is matched against them.
std::vector<SLaunchOption> FindLaunchOptions(Query::SLaunchHints const& hints, std::string_view gameName, SLaunchEnvironment const& environment)
{
	CDesktopEntrySource desktopEntries{};
	CHomeInstallSource homeInstall{};

	desktopEntries.Initialize(environment);
	homeInstall.Initialize(environment);

	std::vector<SLaunchOption> options{ desktopEntries.Find(hints) };

	AppendDistinct(homeInstall.Find(hints, gameName), options);
	homeInstall.Terminate();
	desktopEntries.Terminate();

	return options;
}

//////////////////////////////////////////////////////////////////////////
// game.json's launch facts are Linux's: Windows has nothing to look for until it knows its own.
std::vector<SLaunchOption> FindLaunchOptions(Query::SGameDefinition const& game, SLaunchEnvironment const& environment)
{
#if defined(TGE_PLATFORM_WINDOWS)
	return {};
#elif defined(TGE_PLATFORM_LINUX)
	return FindLaunchOptions(game.launch, game.name, environment);
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
}
} // namespace Lkt::Launch
