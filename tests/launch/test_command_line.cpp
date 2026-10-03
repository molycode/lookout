#include "command_line.hpp"
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
namespace
{
using Arguments = std::vector<std::string>;

//////////////////////////////////////////////////////////////////////////
Arguments SplitValid(std::string_view text, EQuoting quoting)
{
	std::expected<Arguments, ECommandLineError> const arguments{ SplitCommandLine(text, quoting) };

	EXPECT_TRUE(arguments.has_value()) << text;

	return arguments.value_or(Arguments{});
}

//////////////////////////////////////////////////////////////////////////
ECommandLineError SplitError(std::string_view text, EQuoting quoting)
{
	std::expected<Arguments, ECommandLineError> const arguments{ SplitCommandLine(text, quoting) };

	EXPECT_FALSE(arguments.has_value()) << text;

	return arguments.error_or(ECommandLineError::UnknownFieldCode);
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, QuotedPathWithSpacesIsOneArgument)
{
	EXPECT_EQ(SplitValid(R"("/games/my kingpin/run-game.sh" +set name joe)", EQuoting::Strict),
		(Arguments{ "/games/my kingpin/run-game.sh", "+set", "name", "joe" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, RunOfSpacesAddsNoEmptyArgument)
{
	EXPECT_EQ(SplitValid("run   game\t ", EQuoting::Strict), (Arguments{ "run", "game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, EmptyQuotesAreAnEmptyArgument)
{
	EXPECT_EQ(SplitValid(R"(run "" game)", EQuoting::Strict), (Arguments{ "run", "", "game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, QuotedAndUnquotedPartsOfAWordJoin)
{
	EXPECT_EQ(SplitValid(R"(--dir="/my games"/kingpin)", EQuoting::Strict), (Arguments{ "--dir=/my games/kingpin" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, EscapesInsideQuotesAreDecoded)
{
	EXPECT_EQ(SplitValid(R"("\" \` \$ \\")", EQuoting::Strict), (Arguments{ "\" ` $ \\" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, BlankTextGivesNoArguments)
{
	EXPECT_TRUE(SplitValid("  \t ", EQuoting::Strict).empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, UnterminatedQuoteFails)
{
	EXPECT_EQ(SplitError(R"("/games/run-game.sh)", EQuoting::Lenient), ECommandLineError::UnterminatedQuote);
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, BackslashThatEscapesNothingFails)
{
	for (std::string_view const text : { R"("a\b")", R"(a\ b)" })
	{
		SCOPED_TRACE(text);

		EXPECT_EQ(SplitError(text, EQuoting::Lenient), ECommandLineError::StrayBackslash);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, StrictRefusesUnquotedShellCharacters)
{
	for (std::string_view const text : { "~/kingpin/run-game.sh", "run 'my game'", "$HOME/run-game.sh" })
	{
		SCOPED_TRACE(text);

		EXPECT_EQ(SplitError(text, EQuoting::Strict), ECommandLineError::UnquotedShellCharacter);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, StrictAcceptsQuotedShellCharacters)
{
	EXPECT_EQ(SplitValid(R"("~/kingpin" "'a'" "\$HOME")", EQuoting::Strict), (Arguments{ "~/kingpin", "'a'", "$HOME" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, LenientAcceptsUnquotedShellCharacters)
{
	EXPECT_EQ(SplitValid("env LD_LIBRARY_PATH=$LD_LIBRARY_PATH ~/game", EQuoting::Lenient),
		(Arguments{ "env", "LD_LIBRARY_PATH=$LD_LIBRARY_PATH", "~/game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(CommandLine, UnquotedSingleQuoteIsRefusedEvenLeniently)
{
	EXPECT_EQ(SplitError("sh -c 'cd /games && ./run'", EQuoting::Lenient), ECommandLineError::UnquotedShellCharacter);
}

//////////////////////////////////////////////////////////////////////////
TEST(FieldCodes, FileUrlAndIconCodesAreRemoved)
{
	EXPECT_EQ(ExpandFieldCodes(Arguments{ "game", "%U", "--icon=%i", "%f" }, "Kingpin", "/k.desktop"), (Arguments{ "game", "--icon=" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(FieldCodes, NameFileAndPercentExpand)
{
	EXPECT_EQ(ExpandFieldCodes(Arguments{ "%c", "%k", "100%%" }, "Kingpin", "/k.desktop"), (Arguments{ "Kingpin", "/k.desktop", "100%" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(FieldCodes, UnknownCodeFails)
{
	for (std::string_view const argument : { "%z", "50%" })
	{
		SCOPED_TRACE(argument);

		EXPECT_EQ(ExpandFieldCodes(Arguments{ std::string{ argument } }, "Kingpin", "/k.desktop").error_or(ECommandLineError::StrayBackslash),
			ECommandLineError::UnknownFieldCode);
	}
}
} // namespace
} // namespace Lkt::Launch
