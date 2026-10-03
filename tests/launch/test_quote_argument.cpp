#include "command_line.hpp"
#include "launch/quote_argument.hpp"
#include <gtest/gtest.h>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(QuoteArgument, StrictSplittingGivesTheTextBack)
{
	constexpr std::array<std::string_view, 14> Texts{
		"/games/Kingpin/run-game.sh",
		"/home/joe/My Games/kingpin",
		"tab\there",
		"new\nline",
		R"(say "hi")",
		"$HOME/game",
		R"(C:\games\kp.exe)",
		"it's",
		"~/game",
		"a;b|c&d<e>f(g)*?#",
		"100%",
		"`id`",
		"/spiele/König/ゲーム",
		""
	};

	for (std::string_view const text : Texts)
	{
		EXPECT_EQ(SplitCommandLine(QuoteArgument(text), EQuoting::Strict), (std::vector<std::string>{ std::string{ text } })) << text;
	}
}
} // namespace
} // namespace Lkt::Launch
