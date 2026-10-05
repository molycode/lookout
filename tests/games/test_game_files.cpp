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
// As lookout-games has it, which the tests' data folder takes as its download.
std::string ReadDownloadedText(std::string_view key)
{
	std::ifstream file{ std::filesystem::path{ LKT_LOOKOUT_GAMES_DIR } / "games" / key / "game.json", std::ios::binary };

	return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

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
		std::filesystem::create_directory_symlink(LKT_LOOKOUT_GAMES_DIR, GetDownloadedDir(m_dir), error);
		ASSERT_EQ(error.value(), 0) << error.message();
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

	// The downloaded text, changed by edit.
	template<typename TEdit>
	std::string EditDownloaded(std::string_view key, TEdit&& edit) const
	{
		JsonValue game = JsonValue::parse(ReadDownloadedText(key));

		edit(game);

		return game.dump(1, '\t');
	}

	std::filesystem::path m_dir;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, UnchangedDownloadWritesNoFile)
{
	EXPECT_TRUE(SaveGame(m_dir, "quake3", ReadDownloadedText("quake3"), ReadDownloadedText("quake3")).has_value());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "games"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, EditedDownloadWritesOnlyTheChanges)
{
	std::string const text{ EditDownloaded("quake3", [](JsonValue& game) { game["masters"][0]["port"] = 27951; }) };

	ASSERT_TRUE(SaveGame(m_dir, "quake3", text, ReadDownloadedText("quake3")).has_value());

	JsonValue const patch = JsonValue::parse(ReadFile("games/quake3/game.json"));

	ASSERT_TRUE(patch.is_object());
	EXPECT_EQ(patch.size(), 1u);
	EXPECT_TRUE(patch.contains("masters"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, EditedDownloadLoadsAsEdited)
{
	std::string const text{ EditDownloaded("quake3", [](JsonValue& game)
	{
		game.erase("modes");
		game.erase("//foreignServers");
		game.erase("foreignServers");
		game["queryPortOffset"] = 5;
		game["text"]["colourCodes"]["escape"] = "~";
	}) };

	ASSERT_TRUE(SaveGame(m_dir, "quake3", text, ReadDownloadedText("quake3")).has_value());

	SGameContent const content{ LoadGames(GetDownloadedDir(m_dir), m_dir) };
	auto const game{ std::ranges::find(content.games, "quake3", &Query::SGameDefinition::key) };

	EXPECT_TRUE(content.problems.empty()) << content.problems.front().text;
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

	ASSERT_TRUE(SaveGame(m_dir, "quake3", ReadDownloadedText("quake3"), ReadDownloadedText("quake3")).has_value());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "games/quake3"));
}

//////////////////////////////////////////////////////////////////////////
// What the editor opened had another name; the download it is saved over keeps its own.
TEST_F(CGameFilesTest, SaveKeepsWhatTheDownloadChangedSinceOpening)
{
	std::string const opened{ EditDownloaded("quake3", [](JsonValue& game) { game["name"] = "Quake III, an older download"; }) };
	std::string const edited{ EditDownloaded("quake3", [](JsonValue& game)
	{
		game["name"] = "Quake III, an older download";
		game["masters"][0]["port"] = 27951;
	}) };

	ASSERT_TRUE(SaveGame(m_dir, "quake3", edited, opened).has_value());

	JsonValue const merged = JsonValue::parse(ReadGameText(m_dir, "quake3").text);

	EXPECT_EQ(merged["name"], JsonValue::parse(ReadDownloadedText("quake3"))["name"]);
	EXPECT_EQ(merged["masters"][0]["port"], 27951);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, GameNoLongerDownloadedIsSavedWhole)
{
	ASSERT_TRUE(SaveGame(m_dir, "mygame", UserGame, UserGame).has_value());
	EXPECT_EQ(ReadFile("games/mygame/game.json"), UserGame);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, UserGameIsSavedAsWritten)
{
	ASSERT_TRUE(SaveGame(m_dir, "mygame", UserGame, {}).has_value());
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
// As a contributor has it once the game they sent is downloaded: their folder goes whole.
TEST_F(CGameFilesTest, RevertRemovesAnIconTheDownloadCarriesToo)
{
	std::filesystem::path const downloaded{ std::filesystem::path{ LKT_LOOKOUT_GAMES_DIR } / "games/quake3" };

	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");
	std::filesystem::copy_file(downloaded / "icon.png", m_dir / "games/quake3/icon.png");
	std::filesystem::copy_file(downloaded / "icon-licence.txt", m_dir / "games/quake3/icon-licence.txt");

	ASSERT_TRUE(RevertGame(m_dir, "quake3").has_value());
	EXPECT_FALSE(std::filesystem::exists(m_dir / "games/quake3"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, RevertKeepsTheDownloadedIconBesideALicenceOfTheUsers)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");
	std::filesystem::copy_file(std::filesystem::path{ LKT_LOOKOUT_GAMES_DIR } / "games/quake3/icon.png", m_dir / "games/quake3/icon.png");
	WriteFile("games/quake3/icon-licence.txt", "mine");

	ASSERT_TRUE(RevertGame(m_dir, "quake3").has_value());
	EXPECT_TRUE(std::filesystem::exists(m_dir / "games/quake3/icon.png"));
	EXPECT_TRUE(std::filesystem::exists(m_dir / "games/quake3/icon-licence.txt"));
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
TEST_F(CGameFilesTest, PatchedDownloadOpensMerged)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");

	SEditableGame const game{ ReadGameText(m_dir, "quake3") };
	JsonValue const merged = JsonValue::parse(game.text);

	EXPECT_TRUE(game.problem.empty()) << game.problem;
	EXPECT_EQ(merged.value("name", ""), "Quake III, mine");
	EXPECT_TRUE(merged.contains("//foreignServers"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, DownloadedGameOpensWithItsDownload)
{
	WriteFile("games/quake3/game.json", R"json({ "name": "Quake III, mine" })json");

	EXPECT_EQ(ReadGameText(m_dir, "quake3").downloaded, ReadDownloadedText("quake3"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, FolderWithoutADescriptionOpensEmpty)
{
	WriteFile("games/mygame/icon.png", "not looked at");

	SEditableGame const game{ ReadGameText(m_dir, "mygame") };

	EXPECT_TRUE(game.text.empty());
	EXPECT_TRUE(game.problem.empty()) << game.problem;
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
TEST_F(CGameFilesTest, BrokenPatchOpensTheDownloadWithItsProblem)
{
	WriteFile("games/quake3/game.json", "[ 1 ]");

	SEditableGame const game{ ReadGameText(m_dir, "quake3") };

	EXPECT_EQ(game.text, ReadDownloadedText("quake3"));
	EXPECT_EQ(game.problem, "games/quake3/game.json: a change to a downloaded game must be a JSON object");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, DownloadWithoutChangesIsDownloaded)
{
	EXPECT_EQ(FindGameSource(m_dir, "quake3"), EGameSource::Downloaded);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameFilesTest, DownloadWithChangesIsPatched)
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
TEST(GameFiles, EveryDownloadedGamePassesTheCheck)
{
	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		std::expected<void, SFieldProblem> const checked{ CheckGameText(ReadDownloadedText(game.key), Query::GetProtocolCatalog()) };

		EXPECT_TRUE(checked.has_value()) << game.key << ": " << checked.error().path << ": " << checked.error().reason;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(GameFiles, CheckNamesAnUnknownProtocol)
{
	std::string game{ UserGame };

	game.replace(game.find("\"quake2\""), 8, "\"mine\"");

	std::expected<void, SFieldProblem> const checked{ CheckGameText(game, Query::GetProtocolCatalog()) };

	ASSERT_FALSE(checked.has_value());
	EXPECT_EQ(checked.error().path, "protocol");
	EXPECT_TRUE(checked.error().reason.starts_with("'mine' is not one of ")) << checked.error().reason;
}
} // namespace
} // namespace Lkt::Games
