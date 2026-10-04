#include "fixtures.hpp"
#include "games/check_game.hpp"
#include "games/game_files.hpp"
#include "games/load_games.hpp"
#include "json/json.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr std::string_view UserGame{ R"json({
	"//name": "Written by hand, and kept so.",
	"name": "My Game",
	"format": 1,
	"protocol": "quake2",
	"text": { "encoding": "ascii7" },
	"masters": [ { "host": "master.example", "port": 27900 } ],
	"keys": { "hostname": "hostname", "map": "mapname", "maxPlayers": "maxclients", "password": "needpass" },
	"join": { "arguments": [ "+connect", "{address}" ], "passwordArguments": [ "+password", "{password}", "+connect", "{address}" ],
		"password": { "maxLength": 63 } }
})json" };

//////////////////////////////////////////////////////////////////////////
class CGameFilesTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-game-files-XXXXXX").string() };

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

	std::string ReadFile(std::filesystem::path const& relativePath) const
	{
		std::ifstream file{ m_dir / relativePath, std::ios::binary };

		return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
	}

	// The built-in's text as the editor opens it, changed by edit.
	template<typename TEdit>
	std::string EditBuiltin(std::string_view key, TEdit&& edit) const
	{
		JsonValue game = JsonValue::parse(ReadGameText({}, key).text);

		edit(game);

		return game.dump(1, '\t');
	}

	std::filesystem::path m_dir;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, UnchangedBuiltinWritesNoFile)
{
	EXPECT_TRUE(SaveGame(m_dir, "quake3", ReadGameText({}, "quake3").text).has_value());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "games"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, EditedBuiltinWritesOnlyTheChanges)
{
	std::string const text{ EditBuiltin("quake3", [](JsonValue& game) { game["masters"][0]["port"] = 27951; }) };

	ASSERT_TRUE(SaveGame(m_dir, "quake3", text).has_value());

	JsonValue const patch = JsonValue::parse(ReadFile("games/quake3/game.json"));

	ASSERT_TRUE(patch.is_object());
	EXPECT_EQ(patch.size(), 1u);
	EXPECT_TRUE(patch.contains("masters"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, EditedBuiltinLoadsAsEdited)
{
	std::string const text{ EditBuiltin("quake3", [](JsonValue& game)
	{
		game.erase("modes");
		game.erase("//foreignServers");
		game.erase("foreignServers");
		game["queryPortOffset"] = 5;
		game["text"]["colourCodes"]["escape"] = "~";
	}) };

	ASSERT_TRUE(SaveGame(m_dir, "quake3", text).has_value());

	SGameContent const content{ LoadGames(m_dir) };
	auto const game{ std::ranges::find(content.games, "quake3", &Query::SGameDefinition::key) };

	EXPECT_TRUE(content.problems.empty()) << content.problems.front();
	ASSERT_NE(game, content.games.end());
	EXPECT_TRUE(game->modes.empty());
	EXPECT_TRUE(game->foreignServers.empty());
	EXPECT_EQ(game->queryPortOffset, 5);
	EXPECT_EQ(game->text.escape, '~');
	EXPECT_EQ(game->text.palette, Fixtures::GetGameByKey("quake3").text.palette);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, UnchangedSaveRemovesEarlierChanges)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");

	ASSERT_TRUE(SaveGame(m_dir, "quake3", ReadGameText({}, "quake3").text).has_value());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "games/quake3"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, UserGameIsSavedAsWritten)
{
	ASSERT_TRUE(SaveGame(m_dir, "mygame", UserGame).has_value());
	EXPECT_EQ(ReadFile("games/mygame/game.json"), UserGame);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, RevertKeepsTheUsersIcon)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");
	WriteFile("games/quake3/icon.png", "mine");

	ASSERT_TRUE(RevertGame(m_dir, "quake3").has_value());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "games/quake3/game.json"));
	EXPECT_TRUE(std::filesystem::exists(m_dir / "games/quake3/icon.png"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, RemoveDeletesTheWholeFolder)
{
	WriteFile("games/mygame/game.json", UserGame);
	WriteFile("games/mygame/notes.txt", "mine");

	ASSERT_TRUE(RemoveGame(m_dir, "mygame").has_value());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "games/mygame"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, PatchedBuiltinOpensMerged)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");

	SEditableGame const game{ ReadGameText(m_dir, "quake3") };
	JsonValue const merged = JsonValue::parse(game.text);

	EXPECT_TRUE(game.problem.empty()) << game.problem;
	EXPECT_EQ(merged.value("name", ""), "Quake III, mine");
	EXPECT_TRUE(merged.contains("//foreignServers"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, ShortListOfValuesStaysOnOneLine)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");

	std::string const text{ ReadGameText(m_dir, "quake3").text };

	EXPECT_TRUE(text.contains("\n\t\t\t\"palette\": [ \"#000000\", \"#ff0000\", ")) << text;
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, LongListTakesALineAValue)
{
	WriteFile("games/et/game.json", R"json({ "name": "Enemy Territory, mine" })json");

	std::string const text{ ReadGameText(m_dir, "et").text };

	EXPECT_TRUE(text.contains("\n\t\t\t\"palette\": [\n\t\t\t\t\"#000000\",\n\t\t\t\t\"#ff0000\",\n")) << text;
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, BrokenPatchOpensTheBuiltinWithItsProblem)
{
	WriteFile("games/quake3/game.json", "[ 1 ]");

	SEditableGame const game{ ReadGameText(m_dir, "quake3") };

	EXPECT_EQ(game.text, ReadGameText({}, "quake3").text);
	EXPECT_EQ(game.problem, "games/quake3/game.json: a change to a built-in game must be a JSON object");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, BuiltinWithoutChangesIsBuiltin)
{
	EXPECT_EQ(FindGameSource(m_dir, "quake3"), EGameSource::Builtin);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, BuiltinWithChangesIsPatched)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");

	EXPECT_EQ(FindGameSource(m_dir, "quake3"), EGameSource::Patched);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, UserFolderIsUser)
{
	WriteFile("games/mygame/game.json", UserGame);

	EXPECT_EQ(FindGameSource(m_dir, "mygame"), EGameSource::User);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, UnknownKeyIsNone)
{
	EXPECT_EQ(FindGameSource(m_dir, "mygame"), EGameSource::None);
}

//////////////////////////////////////////////////////////////////////////
TEST(GameFiles, KeysOfLowerCaseLettersDigitsDashesAndUnderscoresAreValid)
{
	EXPECT_TRUE(IsValidKey("quake3"));
	EXPECT_TRUE(IsValidKey("2fort_ctf-x"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameFiles, KeysThatAreNoSafeFolderNameAreInvalid)
{
	EXPECT_FALSE(IsValidKey(""));
	EXPECT_FALSE(IsValidKey("-quake"));
	EXPECT_FALSE(IsValidKey("Quake"));
	EXPECT_FALSE(IsValidKey("../quake"));
	EXPECT_FALSE(IsValidKey("my game"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameFiles, EveryBuiltinPassesTheCheck)
{
	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		std::expected<void, std::string> const checked{ CheckGameText(ReadGameText({}, game.key).text, Query::GetProtocolCatalog()) };

		EXPECT_TRUE(checked.has_value()) << game.key << ": " << checked.error();
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(GameFiles, CheckNamesAnUnknownProtocol)
{
	std::string game{ UserGame };

	game.replace(game.find("\"quake2\""), 8, "\"mine\"");

	std::expected<void, std::string> const checked{ CheckGameText(game, Query::GetProtocolCatalog()) };

	ASSERT_FALSE(checked.has_value());
	EXPECT_TRUE(checked.error().starts_with("protocol: 'mine' is not one of ")) << checked.error();
}

//////////////////////////////////////////////////////////////////////////
TEST(GameFiles, NewGameAsksForItsNameFirst)
{
	std::expected<void, std::string> const checked{ CheckGameText(GetNewGameText(), Query::GetProtocolCatalog()) };

	ASSERT_FALSE(checked.has_value());
	EXPECT_EQ(checked.error(), "name: must be a non-empty string without NUL");
}
} // namespace
} // namespace Lkt::Games
