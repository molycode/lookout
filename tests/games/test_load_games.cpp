#include "fixtures.hpp"
#include "script/script_fixture.hpp"
#include "games/load_games.hpp"
#include "query/game_definition.hpp"
#include "query/game_problem.hpp"
#include "query/protocol_definition.hpp"
#include "query/protocol_origin.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Lkt::Games
{
namespace
{
constexpr std::string_view UserGame{ R"json({
	"//format": "A comment, as the downloaded games have them.",
	"format": 1,
	"name": "My Game",
	"protocol": "quake2",
	"text": { "encoding": "ascii7" },
	"masters": [ { "host": "master.example", "port": 27900 } ],
	"keys": { "hostname": "hostname", "map": "mapname", "maxPlayers": "maxclients", "password": "needpass" },
	"join": { "arguments": [ "+connect", "{address}" ], "passwordArguments": [ "+password", "{password}", "+connect", "{address}" ],
		"password": { "maxLength": 63 } }
})json" };

//////////////////////////////////////////////////////////////////////////
Query::SGameDefinition const* FindGame(SGameContent const& content, std::string_view key)
{
	auto const it{ std::ranges::find(content.games, key, &Query::SGameDefinition::key) };

	return (it != content.games.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
Query::SProtocolDefinition const* FindProtocol(SGameContent const& content, std::string_view name)
{
	auto const it{ std::ranges::find(content.protocols, name, &Query::SProtocolDefinition::name) };

	return (it != content.protocols.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
class CLoadGamesTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-games-XXXXXX").string() };

		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_dir = pattern;
	}

	void TearDown() override
	{
		std::error_code error{};

		std::filesystem::remove_all(m_dir, error);
	}
	// ~testing::Test

	void WriteFile(std::filesystem::path const& relativePath, std::string_view text)
	{
		std::filesystem::path const path{ m_dir / relativePath };
		std::error_code error{};

		std::filesystem::create_directories(path.parent_path(), error);

		std::ofstream file{ path, std::ios::binary };

		file << text;
		EXPECT_TRUE(file.good()) << path;
	}

	std::filesystem::path m_dir;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, MissingFolderIsNoProblemAndCreatesNothing)
{
	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir / "absent") };

	EXPECT_TRUE(content.problems.empty());
	EXPECT_EQ(content.games.size(), LoadGames(LKT_LOOKOUT_GAMES_DIR, {}).games.size());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "absent"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, UserGameIsAdded)
{
	WriteFile("games/mygame/game.json", UserGame);

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };
	Query::SGameDefinition const* const pGame{ FindGame(content, "mygame") };

	EXPECT_TRUE(content.problems.empty());
	ASSERT_NE(pGame, nullptr);
	EXPECT_EQ(pGame->name, "My Game");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, DownloadedGameIsPatched)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };
	Query::SGameDefinition const* const pGame{ FindGame(content, "quake3") };

	EXPECT_TRUE(content.problems.empty());
	ASSERT_NE(pGame, nullptr);
	EXPECT_EQ(pGame->name, "Quake III, mine");
	EXPECT_EQ(pGame->masters.size(), Fixtures::GetGameByKey("quake3").masters.size());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, BrokenPatchKeepsTheDownload)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine", "masters": [] })json");

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };
	Query::SGameDefinition const* const pGame{ FindGame(content, "quake3") };

	EXPECT_EQ(content.problems, (std::vector<Query::SGameProblem>{ { "games/quake3/game.json: masters: must list at least one master", "quake3" } }));
	ASSERT_NE(pGame, nullptr);
	EXPECT_EQ(pGame->name, Fixtures::GetGameByKey("quake3").name);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, PatchThatIsNotAnObjectIsAProblem)
{
	WriteFile("games/quake3/game.json", "[ 1 ]");

	EXPECT_EQ(LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir).problems,
		(std::vector<Query::SGameProblem>{ { "games/quake3/game.json: a change to a downloaded game must be a JSON object", "quake3" } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, UserIconReplacesTheDownloadedIcon)
{
	WriteFile("games/kingpin/icon.png", "mine");

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };
	Query::SGameDefinition const* const pGame{ FindGame(content, "kingpin") };

	EXPECT_TRUE(content.problems.empty());
	ASSERT_NE(pGame, nullptr);
	EXPECT_EQ(pGame->icon, Fixtures::ToBytes("mine"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, UserProtocolReplacesTheDownloadedOne)
{
	std::string const source{ Fixtures::GetProtocolByName("quake2").source + "\n-- mine\n" };

	WriteFile("protocols/quake2.lua", source);

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };
	Query::SProtocolDefinition const* const pProtocol{ FindProtocol(content, "quake2") };

	EXPECT_TRUE(content.problems.empty());
	ASSERT_NE(pProtocol, nullptr);
	EXPECT_EQ(pProtocol->source, source);
	EXPECT_EQ(pProtocol->origin, Query::EProtocolOrigin::UserOverDownloaded);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, BrokenUserProtocolKeepsTheDownloadedOne)
{
	WriteFile("protocols/quake2.lua", "return 1");

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };
	Query::SProtocolDefinition const* const pProtocol{ FindProtocol(content, "quake2") };

	ASSERT_EQ(content.problems.size(), 1u);
	EXPECT_TRUE(content.problems.front().text.starts_with("protocols/quake2.lua: ")) << content.problems.front().text;
	EXPECT_TRUE(content.problems.front().key.empty());
	ASSERT_NE(pProtocol, nullptr);
	EXPECT_EQ(pProtocol->source, Fixtures::GetProtocolByName("quake2").source);
	EXPECT_EQ(pProtocol->origin, Query::EProtocolOrigin::Downloaded);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, UserProtocolIsAdded)
{
	std::string game{ UserGame };

	game.replace(game.find("\"quake2\""), 8, "\"mine\"");
	WriteFile("protocols/mine.lua", Fixtures::GetProtocolByName("quake2").source);
	WriteFile("games/mygame/game.json", game);

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };

	EXPECT_TRUE(content.problems.empty());
	EXPECT_NE(FindProtocol(content, "mine"), nullptr);
	EXPECT_NE(FindGame(content, "mygame"), nullptr);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, ProtocolOnlyTheUserHasIsTheirOwn)
{
	WriteFile("protocols/mine.lua", Fixtures::MakeScript(""));

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };
	Query::SProtocolDefinition const* const pProtocol{ FindProtocol(content, "mine") };

	ASSERT_NE(pProtocol, nullptr);
	EXPECT_EQ(pProtocol->origin, Query::EProtocolOrigin::User);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, DownloadedProtocolKnowsItsVersion)
{
	WriteFile("downloaded/protocols/mine.lua", Fixtures::MakeScript("protocol.api = 2 protocol.version = 9041"));

	SGameContent const content{ LoadGames(m_dir / "downloaded", m_dir) };
	Query::SProtocolDefinition const* const pProtocol{ FindProtocol(content, "mine") };

	ASSERT_NE(pProtocol, nullptr);
	EXPECT_EQ(pProtocol->downloadedVersion, 9041u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, UserProtocolOverADownloadedOneKnowsTheDownloadedVersion)
{
	std::string const source{ Fixtures::MakeScript("-- mine") };

	WriteFile("downloaded/protocols/mine.lua", Fixtures::MakeScript("protocol.api = 2 protocol.version = 9041"));
	WriteFile("protocols/mine.lua", source);

	SGameContent const content{ LoadGames(m_dir / "downloaded", m_dir) };
	Query::SProtocolDefinition const* const pProtocol{ FindProtocol(content, "mine") };

	ASSERT_NE(pProtocol, nullptr);
	ASSERT_EQ(pProtocol->source, source);
	EXPECT_EQ(pProtocol->downloadedVersion, 9041u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, BrokenDownloadedProtocolUnderAUserOneIsAProblem)
{
	WriteFile("downloaded/protocols/mine.lua", "return 1");
	WriteFile("protocols/mine.lua", Fixtures::MakeScript(""));

	SGameContent const content{ LoadGames(m_dir / "downloaded", m_dir) };

	ASSERT_EQ(content.problems.size(), 1u);
	EXPECT_TRUE(content.problems.front().text.starts_with("downloaded/protocols/mine.lua: ")) << content.problems.front().text;
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, FolderNameThatIsNotAKeyIsAProblem)
{
	WriteFile("games/My Game/game.json", UserGame);

	EXPECT_EQ(LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir).problems, (std::vector<Query::SGameProblem>{ { "games/My Game: its name must use only lower-case letters, digits, '-' and '_'", "" } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, DotEntriesAreSkipped)
{
	WriteFile("games/.git/game.json", "not json");
	WriteFile("protocols/.hidden.lua", "return 1");

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };

	EXPECT_TRUE(content.problems.empty());
	EXPECT_EQ(content.games.size(), LoadGames(LKT_LOOKOUT_GAMES_DIR, {}).games.size());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, UserFolderWithoutAGameIsAProblem)
{
	WriteFile("games/empty/notes.txt", "");

	EXPECT_EQ(LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir).problems, (std::vector<Query::SGameProblem>{ { "games/empty: has no game.json", "empty" } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, UnusableUserGameIsLeftOutAndNamesItsKey)
{
	WriteFile("games/mygame/game.json", R"json({ "format": 1, "name": "My Game" })json");

	SGameContent const content{ LoadGames(LKT_LOOKOUT_GAMES_DIR, m_dir) };

	ASSERT_EQ(content.problems.size(), 1u);
	EXPECT_EQ(content.problems.front().key, "mygame");
	EXPECT_EQ(FindGame(content, "mygame"), nullptr);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, NothingDownloadedIsNoGames)
{
	SGameContent const content{ LoadGames(m_dir / "downloaded", {}) };

	EXPECT_TRUE(content.games.empty());
	EXPECT_TRUE(content.problems.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLoadGamesTest, BrokenDownloadIsLeftOutAndNamesNoGame)
{
	WriteFile("downloaded/games/mygame/game.json", "{ \"format\": 1 }");

	SGameContent const content{ LoadGames(m_dir / "downloaded", {}) };

	ASSERT_EQ(content.problems.size(), 1u);
	EXPECT_TRUE(content.problems.front().text.starts_with("downloaded/games/mygame/game.json: ")) << content.problems.front().text;
	EXPECT_TRUE(content.problems.front().key.empty());
	EXPECT_TRUE(content.games.empty());
}
} // namespace
} // namespace Lkt::Games
