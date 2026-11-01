#include "launch/command_option.hpp"
#include "launch/quote_argument.hpp"
#include <gtest/gtest.h>
#include <tge/platform.hpp>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
namespace
{
#if defined(TGE_PLATFORM_WINDOWS)
constexpr std::string_view ShellCommand{ "kingpin.exe > out.txt" };
constexpr std::string_view AbsoluteProgram{ R"(C:\Games\Kingpin\kingpin.exe)" };
constexpr std::string_view RelativeProgram{ R"(Kingpin\kingpin.exe)" };
#elif defined(TGE_PLATFORM_LINUX)
constexpr std::string_view ShellCommand{ "~/kingpin/run-game.sh" };
constexpr std::string_view AbsoluteProgram{ "/games/kingpin/kingpin" };
constexpr std::string_view RelativeProgram{ "kingpin/kingpin" };
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

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
	EXPECT_EQ(MakeCommandOption(ShellCommand).error_or(ELaunchError::SpawnFailed), ELaunchError::BadCustomCommand);
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandOption, PickedProgramIsItsOwnCommand)
{
	std::expected<SLaunchOption, ELaunchError> const option{ MakeCommandOption(QuoteArgument(AbsoluteProgram)) };

	ASSERT_TRUE(option.has_value());
	EXPECT_EQ(option->argv, (std::vector<std::string>{ std::string{ AbsoluteProgram } }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandOption, ProgramGivenByItsFullPathStartsInItsFolder)
{
	std::expected<SLaunchOption, ELaunchError> const option{ MakeCommandOption(std::format("{} +set name joe", QuoteArgument(AbsoluteProgram))) };

	ASSERT_TRUE(option.has_value());
	EXPECT_EQ(std::filesystem::path{ option->workingDir }, std::filesystem::path{ AbsoluteProgram }.parent_path());
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandOption, RelativeProgramKeepsLookoutsFolder)
{
	std::expected<SLaunchOption, ELaunchError> const option{ MakeCommandOption(QuoteArgument(RelativeProgram)) };

	ASSERT_TRUE(option.has_value());
	EXPECT_TRUE(option->workingDir.empty());
}
} // namespace
} // namespace Lkt::Launch
