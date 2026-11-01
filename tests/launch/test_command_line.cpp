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
Arguments SplitValid(std::string_view text, ECommandSyntax syntax, EQuoting quoting)
{
	std::expected<Arguments, ECommandLineError> const arguments{ SplitCommandLine(text, syntax, quoting) };

	EXPECT_TRUE(arguments.has_value()) << text;

	return arguments.value_or(Arguments{});
}

//////////////////////////////////////////////////////////////////////////
ECommandLineError SplitError(std::string_view text, ECommandSyntax syntax, EQuoting quoting)
{
	std::expected<Arguments, ECommandLineError> const arguments{ SplitCommandLine(text, syntax, quoting) };

	EXPECT_FALSE(arguments.has_value()) << text;

	return arguments.error_or(ECommandLineError::UnknownFieldCode);
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, QuotedPathWithSpacesIsOneArgument)
{
	EXPECT_EQ(SplitValid(R"("/games/my kingpin/run-game.sh" +set name joe)", ECommandSyntax::Posix, EQuoting::Strict),
		(Arguments{ "/games/my kingpin/run-game.sh", "+set", "name", "joe" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, RunOfSpacesAddsNoEmptyArgument)
{
	EXPECT_EQ(SplitValid("run   game\t ", ECommandSyntax::Posix, EQuoting::Strict), (Arguments{ "run", "game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, EmptyQuotesAreAnEmptyArgument)
{
	EXPECT_EQ(SplitValid(R"(run "" game)", ECommandSyntax::Posix, EQuoting::Strict), (Arguments{ "run", "", "game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, QuotedAndUnquotedPartsOfAWordJoin)
{
	EXPECT_EQ(SplitValid(R"(--dir="/my games"/kingpin)", ECommandSyntax::Posix, EQuoting::Strict), (Arguments{ "--dir=/my games/kingpin" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, EscapesInsideQuotesAreDecoded)
{
	EXPECT_EQ(SplitValid(R"("\" \` \$ \\")", ECommandSyntax::Posix, EQuoting::Strict), (Arguments{ "\" ` $ \\" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, BlankTextGivesNoArguments)
{
	EXPECT_TRUE(SplitValid("  \t ", ECommandSyntax::Posix, EQuoting::Strict).empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, UnterminatedQuoteFails)
{
	EXPECT_EQ(SplitError(R"("/games/run-game.sh)", ECommandSyntax::Posix, EQuoting::Lenient), ECommandLineError::UnterminatedQuote);
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, BackslashThatEscapesNothingFails)
{
	for (std::string_view const text : { R"("a\b")", R"(a\ b)" })
	{
		SCOPED_TRACE(text);

		EXPECT_EQ(SplitError(text, ECommandSyntax::Posix, EQuoting::Lenient), ECommandLineError::StrayBackslash);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, StrictRefusesUnquotedShellCharacters)
{
	for (std::string_view const text : { "~/kingpin/run-game.sh", "run 'my game'", "$HOME/run-game.sh" })
	{
		SCOPED_TRACE(text);

		EXPECT_EQ(SplitError(text, ECommandSyntax::Posix, EQuoting::Strict), ECommandLineError::UnquotedShellCharacter);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, StrictAcceptsQuotedShellCharacters)
{
	EXPECT_EQ(SplitValid(R"("~/kingpin" "'a'" "\$HOME")", ECommandSyntax::Posix, EQuoting::Strict), (Arguments{ "~/kingpin", "'a'", "$HOME" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, LenientAcceptsUnquotedShellCharacters)
{
	EXPECT_EQ(SplitValid("env LD_LIBRARY_PATH=$LD_LIBRARY_PATH ~/game", ECommandSyntax::Posix, EQuoting::Lenient),
		(Arguments{ "env", "LD_LIBRARY_PATH=$LD_LIBRARY_PATH", "~/game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(PosixCommandLine, UnquotedSingleQuoteIsRefusedEvenLeniently)
{
	EXPECT_EQ(SplitError("sh -c 'cd /games && ./run'", ECommandSyntax::Posix, EQuoting::Lenient), ECommandLineError::UnquotedShellCharacter);
}

//////////////////////////////////////////////////////////////////////////
TEST(WindowsCommandLine, PathsKeepTheirBackslashes)
{
	EXPECT_EQ(SplitValid(R"("C:\Games\My Kingpin\kingpin.exe" +set name joe C:\kp\main)", ECommandSyntax::Windows, EQuoting::Strict),
		(Arguments{ R"(C:\Games\My Kingpin\kingpin.exe)", "+set", "name", "joe", R"(C:\kp\main)" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(WindowsCommandLine, RunOfSpacesAddsNoEmptyArgument)
{
	EXPECT_EQ(SplitValid("run   game\t ", ECommandSyntax::Windows, EQuoting::Strict), (Arguments{ "run", "game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(WindowsCommandLine, EmptyQuotesAreAnEmptyArgument)
{
	EXPECT_EQ(SplitValid(R"(run "" game)", ECommandSyntax::Windows, EQuoting::Strict), (Arguments{ "run", "", "game" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(WindowsCommandLine, BackslashesBeforeAQuoteAreHalved)
{
	EXPECT_EQ(SplitValid(R"("a\"b" "c\\" d\\\"e)", ECommandSyntax::Windows, EQuoting::Strict), (Arguments{ "a\"b", "c\\", "d\\\"e" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(WindowsCommandLine, DoubledQuoteInsideQuotesIsAQuote)
{
	EXPECT_EQ(SplitValid(R"("say ""hi""")", ECommandSyntax::Windows, EQuoting::Strict), (Arguments{ "say \"hi\"" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(WindowsCommandLine, UnterminatedQuoteFails)
{
	EXPECT_EQ(SplitError(R"("C:\Games\kingpin.exe)", ECommandSyntax::Windows, EQuoting::Strict), ECommandLineError::UnterminatedQuote);
}

//////////////////////////////////////////////////////////////////////////
// cmd.exe's characters: no shell acts on them here, so a command that leans on one would not do what it says.
TEST(WindowsCommandLine, StrictRefusesUnquotedShellCharacters)
{
	for (std::string_view const text : { "game.exe & other.exe", "game.exe > out.txt", "%USERPROFILE%\\game.exe", "a^b" })
	{
		SCOPED_TRACE(text);

		EXPECT_EQ(SplitError(text, ECommandSyntax::Windows, EQuoting::Strict), ECommandLineError::UnquotedShellCharacter);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(WindowsCommandLine, StrictAcceptsQuotedShellCharactersAndPosixOnes)
{
	EXPECT_EQ(SplitValid(R"("a & b" "100%" it's ~/x $HOME)", ECommandSyntax::Windows, EQuoting::Strict),
		(Arguments{ "a & b", "100%", "it's", "~/x", "$HOME" }));
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
