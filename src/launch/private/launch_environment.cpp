#include "launch/launch_environment.hpp"
#include <tge/platform.hpp>

#if defined(TGE_PLATFORM_WINDOWS)
#include "loggers.hpp"
#include "registry.hpp"
#include "steam_libraries.hpp"
#include <string_view>
#include <system_error>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#elif defined(TGE_PLATFORM_LINUX)
#include "path_list.hpp"
#include <cstdlib>
#include <string_view>
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

namespace Lkt::Launch
{
namespace
{
#if defined(TGE_PLATFORM_WINDOWS)
//////////////////////////////////////////////////////////////////////////
// Windows' own folder, never Lookout's: an XDG variable moves Lookout's files, not where games are installed.
std::filesystem::path ReadKnownFolder(KNOWNFOLDERID const& id, std::string_view name)
{
	PWSTR pPath{ nullptr };
	HRESULT const result{ SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &pPath) };
	std::filesystem::path folder{};

	if (SUCCEEDED(result))
	{
		folder = pPath;
	}
	else
	{
		gLog.Warning("Cannot locate the {} folder, so no game is looked for there: {}", name, std::system_category().message(static_cast<int>(result)));
	}

	CoTaskMemFree(pPath);

	return folder;
}
#elif defined(TGE_PLATFORM_LINUX)
constexpr std::string_view DefaultDataDirs{ "/usr/local/share:/usr/share" };
constexpr std::string_view DefaultSearchPath{ "/usr/local/bin:/usr/bin:/bin" };

//////////////////////////////////////////////////////////////////////////
std::string_view GetEnvironment(char const* pName)
{
	char const* const pValue{ std::getenv(pName) };

	return (pValue != nullptr) ? std::string_view{ pValue } : std::string_view{};
}
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace

//////////////////////////////////////////////////////////////////////////
// Read once, as Lookout starts, so a game installed later is found on the next start. On Linux, the XDG Base Directory
// defaults, where an empty or relative value counts as unset.
SLaunchEnvironment ReadLaunchEnvironment()
{
	SLaunchEnvironment environment{};

#if defined(TGE_PLATFORM_WINDOWS)
	std::filesystem::path steamFolder{ ReadRegistryFolder(ERegistryRoot::CurrentUser, "Software\\Valve\\Steam", "SteamPath") };

	if (steamFolder.empty())
	{
		steamFolder = ReadRegistryFolder(ERegistryRoot::LocalMachine, "SOFTWARE\\Valve\\Steam", "InstallPath");
	}

	environment.programFiles = ReadKnownFolder(FOLDERID_ProgramFiles, "Program Files");
	environment.programFilesX86 = ReadKnownFolder(FOLDERID_ProgramFilesX86, "Program Files (x86)");
	environment.localAppData = ReadKnownFolder(FOLDERID_LocalAppData, "Local AppData");
	environment.systemDrive = ReadKnownFolder(FOLDERID_Windows, "Windows").root_path();
	environment.steamLibraries = ReadSteamLibraries(steamFolder.make_preferred());
	environment.gogInstalls = ReadGogInstalls(ERegistryRoot::LocalMachine, "SOFTWARE\\GOG.com\\Games");
#elif defined(TGE_PLATFORM_LINUX)
	std::filesystem::path const home{ GetEnvironment("HOME") };
	std::filesystem::path const dataHome{ GetEnvironment("XDG_DATA_HOME") };
	std::string_view const dataDirs{ GetEnvironment("XDG_DATA_DIRS") };
	std::string_view const searchPath{ GetEnvironment("PATH") };

	if (home.is_absolute())
	{
		environment.home = home;
	}

	if (dataHome.is_absolute())
	{
		environment.dataHome = dataHome;
	}
	else if (!environment.home.empty())
	{
		environment.dataHome = environment.home / ".local/share";
	}

	environment.dataDirs = SplitPathList(dataDirs.empty() ? DefaultDataDirs : dataDirs);
	environment.searchPath = searchPath.empty() ? DefaultSearchPath : searchPath;
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

	return environment;
}
} // namespace Lkt::Launch
