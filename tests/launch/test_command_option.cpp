#include "launch/command_option.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(CommandOption, CommandIsSplit)
{
	std::expected<SLaunchOption, ELaunchError> const option{ MakeCommandOption("steam -applaunch 38430") };

	ASSERT_TRUE(option.has_value());
	EXPECT_EQ(option->argv, (std::vector<std::string>{ "steam", "-applaunch", "38430" }));
	EXPECT_TRUE(option->workingDir.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandOption, BlankCommandIsEmpty)
{
	EXPECT_EQ(MakeCommandOption("   ").error_or(ELaunchError::SpawnFailed), ELaunchError::EmptyCustomCommand);
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandOption, CommandWithAnEmptyProgramIsEmpty)
{
	EXPECT_EQ(MakeCommandOption("\"\" -applaunch 38430").error_or(ELaunchError::SpawnFailed), ELaunchError::EmptyCustomCommand);
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandOption, ShellSyntaxIsABadCommand)
{
	EXPECT_EQ(MakeCommandOption("~/kingpin/run-game.sh").error_or(ELaunchError::SpawnFailed), ELaunchError::BadCustomCommand);
}
} // namespace
} // namespace Lkt::Launch
