#include "launch/launch_environment.hpp"
#include "path_list.hpp"
#include <cstdlib>
#include <string_view>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view DefaultDataDirs{ "/usr/local/share:/usr/share" };
constexpr std::string_view DefaultSearchPath{ "/usr/local/bin:/usr/bin:/bin" };

//////////////////////////////////////////////////////////////////////////
std::string_view GetEnvironment(char const* pName)
{
	char const* const pValue{ std::getenv(pName) };

	return (pValue != nullptr) ? std::string_view{ pValue } : std::string_view{};
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// The XDG Base Directory defaults, where an empty or relative value counts as unset.
SLaunchEnvironment ReadLaunchEnvironment()
{
	SLaunchEnvironment environment{};
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

	return environment;
}
} // namespace Lkt::Launch
