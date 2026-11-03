#include "launch/launch_discovery.hpp"
#include "desktop_entry_source.hpp"
#include "folder_candidates_source.hpp"
#include "gog_source.hpp"
#include "hinted_discovery.hpp"
#include "home_install_source.hpp"
#include "steam_source.hpp"
#include "store_source.hpp"
#include "query/game_definition.hpp"
#include <tge/platform.hpp>
#include <algorithm>
#include <filesystem>
#include <iterator>
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
// Through a store after the rest: those start the game itself, where Lookout keeps its output.
std::vector<SLaunchOption> FindLaunchOptions(Query::SLaunchHints const& hints, std::string_view gameName, SLaunchEnvironment const& environment,
	std::span<SFoundStore const> stores)
{
	std::vector<SLaunchOption> options{};
	CStoreSource storeSource{};

#if defined(TGE_PLATFORM_WINDOWS)
	CSteamSource steam{};
	CGogSource gog{};
	CFolderCandidatesSource folders{};

	steam.Initialize(environment);
	gog.Initialize(environment);
	folders.Initialize(environment);
	AppendDistinct(steam.Find(hints, gameName), options);
	AppendDistinct(gog.Find(hints, gameName), options);
	AppendDistinct(folders.Find(hints, gameName), options);
	folders.Terminate();
	gog.Terminate();
	steam.Terminate();
#elif defined(TGE_PLATFORM_LINUX)
	CDesktopEntrySource desktopEntries{};
	CHomeInstallSource homeInstall{};

	desktopEntries.Initialize(environment);
	homeInstall.Initialize(environment);
	// Desktop entries may share a wrapper such as flatpak, so only the install folder is matched against them.
	options = desktopEntries.Find(hints);
	AppendDistinct(homeInstall.Find(hints, gameName), options);
	homeInstall.Terminate();
	desktopEntries.Terminate();
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

	storeSource.Initialize(stores);
	std::ranges::move(storeSource.Find(hints, gameName), std::back_inserter(options));
	storeSource.Terminate();

	return options;
}

//////////////////////////////////////////////////////////////////////////
std::vector<SLaunchOption> FindLaunchOptions(Query::SGameDefinition const& game, SLaunchEnvironment const& environment, std::span<SFoundStore const> stores)
{
	return FindLaunchOptions(game.launch, game.name, environment, stores);
}
} // namespace Lkt::Launch
