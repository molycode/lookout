#include "launch/launch_environment.hpp"
#include <tge/platform.hpp>
#include <gtest/gtest.h>
#include <filesystem>

namespace Lkt::Launch
{
namespace
{
// Windows' folders for its usual install places; Linux's environment is the XDG one, tested through discovery.
#if defined(TGE_PLATFORM_WINDOWS)
//////////////////////////////////////////////////////////////////////////
TEST(LaunchEnvironment, KnownFoldersAreAbsolute)
{
	SLaunchEnvironment const environment{ ReadLaunchEnvironment() };

	EXPECT_TRUE(environment.programFiles.is_absolute()) << environment.programFiles.string();
	EXPECT_TRUE(environment.programFilesX86.is_absolute()) << environment.programFilesX86.string();
	EXPECT_TRUE(environment.localAppData.is_absolute()) << environment.localAppData.string();
}

//////////////////////////////////////////////////////////////////////////
// "C:" alone would make "C:" / "Games" the drive-relative "C:Games".
TEST(LaunchEnvironment, SystemDriveIsItsRoot)
{
	SLaunchEnvironment const environment{ ReadLaunchEnvironment() };

	EXPECT_TRUE(environment.systemDrive.has_root_directory()) << environment.systemDrive.string();
	EXPECT_EQ(environment.systemDrive, environment.systemDrive.root_path());
}
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace
} // namespace Lkt::Launch
