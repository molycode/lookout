#include "parse_command_line.hpp"
#include <gtest/gtest.h>
#include <array>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace Lkt::Cli;

namespace
{
constexpr std::array<std::string_view, 2> GameKeys{ "kingpin", "quake2" };

//////////////////////////////////////////////////////////////////////////
std::expected<SRunRequest, std::string> Parse(std::vector<std::string_view> const& options)
{
	return ParseCommandLine(options, GameKeys);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, NoOptionsOpenTheWindow)
{
	std::expected<SRunRequest, std::string> const request{ Parse({}) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Window);
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, ListNamesItsGame)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--list", "quake2" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::List);
	EXPECT_EQ(request->listGame, "quake2");
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, ListRefusesAnUnknownGame)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--list", "nosuch" }) };

	ASSERT_FALSE(request.has_value());
	EXPECT_EQ(request.error(), "Unknown game 'nosuch' - expected one of: kingpin, quake2");
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, ListTakesTheNextWordAsItsGame)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--list", "--help" }) };

	ASSERT_FALSE(request.has_value());
	EXPECT_EQ(request.error(), "Unknown game '--help' - expected one of: kingpin, quake2");
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, ListNeedsAGame)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--list" }) };

	ASSERT_FALSE(request.has_value());
	EXPECT_EQ(request.error(), "--list needs a game, one of: kingpin, quake2");
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, WithoutGamesListSaysHowToGetThem)
{
	std::vector<std::string_view> const options{ "--list" };
	std::expected<SRunRequest, std::string> const request{ ParseCommandLine(options, {}) };

	ASSERT_FALSE(request.has_value());
	EXPECT_EQ(request.error(), "--list needs a game, one of: none, as no game is installed (--download gets them)");
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, DownloadWithoutGamesTakesEveryUpdate)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--download" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Download);
	EXPECT_TRUE(request->downloadKeys.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, DownloadTakesEveryWordAfterIt)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--download", "quake2", "--list", "kingpin", "ut2004" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->downloadKeys, (std::vector<std::string>{ "quake2", "ut2004" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, DownloadWinsOverList)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--list", "quake2", "--download" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Download);
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, CheckNamesItsFolder)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--check", "lookout-games" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Check);
	EXPECT_EQ(request->checkFolder, std::filesystem::path{ "lookout-games" });
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, CheckNeedsAFolder)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--check" }) };

	ASSERT_FALSE(request.has_value());
	EXPECT_EQ(request.error(), "--check needs the folder to check");
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, CheckWinsOverDownload)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--download", "--check", "lookout-games" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Check);
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, VersionWinsOverCheck)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--check", "lookout-games", "--version" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Version);
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, HelpWinsOverVersion)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--version", "--help" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Help);
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, HelpWinsOverAProblemAfterIt)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--help", "--nonsense" }) };

	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->command, ECommand::Help);
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, AProblemEndsReadingBeforeHelp)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "--nonsense", "--help" }) };

	ASSERT_FALSE(request.has_value());
	EXPECT_EQ(request.error(), "Unknown option '--nonsense' - try --help");
}

//////////////////////////////////////////////////////////////////////////
TEST(ParseCommandLine, AWordWithoutDownloadIsUnknown)
{
	std::expected<SRunRequest, std::string> const request{ Parse({ "quake2" }) };

	ASSERT_FALSE(request.has_value());
	EXPECT_EQ(request.error(), "Unknown option 'quake2' - try --help");
}
