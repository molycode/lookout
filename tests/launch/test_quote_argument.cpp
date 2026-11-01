#include "command_line.hpp"
#include <gtest/gtest.h>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
namespace
{
constexpr std::array<std::string_view, 17> Texts{
	"/games/Kingpin/run-game.sh",
	"/home/joe/My Games/kingpin",
	R"(C:\Games\My Kingpin\kingpin.exe)",
	"tab\there",
	"new\nline",
	R"(say "hi")",
	R"(trailing\)",
	R"(before\"quote)",
	"$HOME/game",
	"it's",
	"~/game",
	"a;b|c&d<e>f(g)*?#^",
	"100%",
	"%USERPROFILE%",
	"`id`",
	"/spiele/König/ゲーム",
	""
};

//////////////////////////////////////////////////////////////////////////
TEST(QuoteArgument, StrictPosixSplittingGivesTheTextBack)
{
	for (std::string_view const text : Texts)
	{
		EXPECT_EQ(SplitCommandLine(QuoteArgument(text, ECommandSyntax::Posix), ECommandSyntax::Posix, EQuoting::Strict),
			(std::vector<std::string>{ std::string{ text } })) << text;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(QuoteArgument, StrictWindowsSplittingGivesTheTextBack)
{
	for (std::string_view const text : Texts)
	{
		EXPECT_EQ(SplitCommandLine(QuoteArgument(text, ECommandSyntax::Windows), ECommandSyntax::Windows, EQuoting::Strict),
			(std::vector<std::string>{ std::string{ text } })) << text;
	}
}

//////////////////////////////////////////////////////////////////////////
// What a picked program is stored as: a Windows path reads as it is, with no doubled backslashes.
TEST(QuoteArgument, WindowsPathIsOnlyWrapped)
{
	EXPECT_EQ(QuoteArgument(R"(C:\Games\Kingpin\kingpin.exe)", ECommandSyntax::Windows), R"("C:\Games\Kingpin\kingpin.exe")");
}
} // namespace
} // namespace Lkt::Launch
