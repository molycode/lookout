#include "launcher_choice.hpp"
#include "launch/launcher_ids.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace Lkt::Browser
{
namespace
{
using Launch::ELaunchError;
using Launch::SLaunchOption;

//////////////////////////////////////////////////////////////////////////
std::vector<SLaunchOption> MakeDiscovered()
{
	return std::vector<SLaunchOption>{
		SLaunchOption{ "kingpin-native.desktop", "Kingpin: Life of Crime", "/games/Kingpin", { "/games/Kingpin/run-game.sh" }, "/games/Kingpin" },
		SLaunchOption{ std::string{ Launch::InstallDirLauncherId }, "Kingpin: Life of Crime", "~/Games/Kingpin", { "/home/joe/Games/Kingpin/run-game.sh" }, "/home/joe/Games/Kingpin" }
	};
}

//////////////////////////////////////////////////////////////////////////
std::vector<SInstallLauncher> MakeInstalls()
{
	SLaunchOption const steam{ "install:2", "Steam", "steam -applaunch 38430", { "steam", "-applaunch", "38430" }, {} };

	return std::vector<SInstallLauncher>{
		SInstallLauncher{ "install:1", "Old copy", "/opt/kingpin", std::unexpected{ ELaunchError::BrokenInstallFolder } },
		SInstallLauncher{ "install:2", "Steam", "steam -applaunch 38430", steam }
	};
}

//////////////////////////////////////////////////////////////////////////
TEST(LauncherChoice, NoIdTakesTheFirstDiscovered)
{
	std::vector<SLaunchOption> const discovered{ MakeDiscovered() };

	EXPECT_EQ(ChooseLauncher(discovered, MakeInstalls(), ""), discovered.front());
}

//////////////////////////////////////////////////////////////////////////
TEST(LauncherChoice, NoIdFallsBackToTheFirstInstallThatCanStart)
{
	std::vector<SInstallLauncher> const installs{ MakeInstalls() };

	EXPECT_EQ(ChooseLauncher({}, installs, ""), installs.back().option);
}

//////////////////////////////////////////////////////////////////////////
TEST(LauncherChoice, NothingFoundIsNoLauncher)
{
	EXPECT_EQ(ChooseLauncher({}, {}, "").error_or(ELaunchError::SpawnFailed), ELaunchError::NoLauncher);
}

//////////////////////////////////////////////////////////////////////////
TEST(LauncherChoice, ChosenDiscoveredLauncherIsFound)
{
	std::vector<SLaunchOption> const discovered{ MakeDiscovered() };

	EXPECT_EQ(ChooseLauncher(discovered, {}, Launch::InstallDirLauncherId), discovered.back());
}

//////////////////////////////////////////////////////////////////////////
TEST(LauncherChoice, MissingChosenLauncherIsReported)
{
	EXPECT_EQ(ChooseLauncher(MakeDiscovered(), MakeInstalls(), "steam.desktop").error_or(ELaunchError::SpawnFailed), ELaunchError::ChosenLauncherMissing);
}

//////////////////////////////////////////////////////////////////////////
TEST(LauncherChoice, ChosenInstallIsUsed)
{
	std::vector<SInstallLauncher> const installs{ MakeInstalls() };

	EXPECT_EQ(ChooseLauncher(MakeDiscovered(), installs, "install:2"), installs.back().option);
}

//////////////////////////////////////////////////////////////////////////
TEST(LauncherChoice, ChosenBrokenInstallReportsItsError)
{
	EXPECT_EQ(ChooseLauncher(MakeDiscovered(), MakeInstalls(), "install:1").error_or(ELaunchError::SpawnFailed), ELaunchError::BrokenInstallFolder);
}
} // namespace
} // namespace Lkt::Browser
