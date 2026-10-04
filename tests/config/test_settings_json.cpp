#include "catalog_fixture.hpp"
#include "empty_catalog_fixture.hpp"
#include "fixtures.hpp"
#include "settings_json.hpp"
#include "config/default_settings.hpp"
#include "config/settings.hpp"
#include "json/json.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/server_address.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Lkt::Config
{
namespace
{
using CSettingsWithoutGamesTest = Fixtures::CEmptyCatalogTest;

// 203.0.113.7:31510 and 203.0.113.8:27910.
constexpr Query::SServerAddress Favourite{ 0xCB007107, 31510 };
constexpr Query::SServerAddress OtherFavourite{ 0xCB007108, 27910 };

constexpr std::array SortColumns{ ESortColumn::Name, ESortColumn::Map, ESortColumn::Mod, ESortColumn::Mode, ESortColumn::Ping };

//////////////////////////////////////////////////////////////////////////
// Values differ between games and between neighbouring fields, and each leaves its default somewhere, so a field
// dropped or crossed shows.
SSettings MakeVariedSettings()
{
	SSettings settings{ MakeDefaultSettings() };

	settings.window = SWindowSettings{ 1600, 1000, true, 360, "[Table][0x1A2B3C4D,8]\nColumn 0  Width=40\n" };
	// Odd positions are listed below, and the first is the default.
	settings.selectedGame = Query::GetGameCatalog()[1].game;
	settings.autoRefreshSeconds = 45;
	std::ranges::reverse(settings.gameOrder);

	for (size_t index{ 0 }; index < settings.games.size(); ++index)
	{
		SGameSettings& game{ settings.games[index] };
		bool const isEven{ index % 2 == 0 };
		uint32_t const offset{ static_cast<uint32_t>(index) };

		game.isListed = !isEven;
		game.filter = SServerFilter{ std::format("search {}", index), isEven, !isEven, 100 + offset, std::format("mod {}", index),
			std::format("C{}", index) };
		game.sort = SSortOrder{ SortColumns[index % SortColumns.size()], isEven };
		game.installs = {
			SGameInstall{ 1 + offset, std::format("Copy {}", index), EInstallKind::Command, std::format("run-game-{} +connect", index) },
			SGameInstall{ 7 + offset, {}, EInstallKind::Folder, std::format("/games/{}", index) }
		};
		game.favourites = { Query::SServerAddress{ Favourite.ipv4 + offset, Favourite.port }, OtherFavourite };
	}

	return settings;
}

//////////////////////////////////////////////////////////////////////////
SSettingsDocument ReadValid(std::string_view text)
{
	std::expected<SSettingsDocument, ESettingsJsonError> const document{ ReadSettingsJson(text) };

	EXPECT_TRUE(document.has_value()) << text;

	return document.value_or(SSettingsDocument{});
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, EveryValueRoundTrips)
{
	SSettings const settings{ MakeVariedSettings() };
	SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

	EXPECT_EQ(document.settings, settings);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, DefaultsRoundTrip)
{
	SSettingsDocument const document{ ReadValid(WriteSettingsJson(MakeDefaultSettings())) };

	EXPECT_EQ(document.settings, MakeDefaultSettings());
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, EmptyObjectGivesDefaults)
{
	SSettingsDocument const document{ ReadValid("{}") };

	EXPECT_EQ(document.settings, MakeDefaultSettings());
	EXPECT_EQ(document.version, SettingsVersion);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, UnknownKeysAreIgnored)
{
	SSettingsDocument const document{ ReadValid(R"({ "colour": "amber", "games": { "doom": { "listed": 7 } } })") };

	EXPECT_EQ(document.settings, MakeDefaultSettings());
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, NewerVersionIsReported)
{
	EXPECT_EQ(ReadValid(R"({ "version": 7 })").version, 7u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, WidthThatIsNoUnsignedIntegerIsRejected)
{
	for (std::string_view const width : { "true", "-1", "1280.5", "1e300", "\"wide\"", "null" })
	{
		SCOPED_TRACE(width);

		SSettingsDocument const document{ ReadValid(std::format(R"({{ "window": {{ "width": {}, "height": 900 }} }})", width)) };

		EXPECT_EQ(document.settings.window.width, SWindowSettings{}.width);
		EXPECT_EQ(document.settings.window.height, 900u);
		EXPECT_EQ(document.numInvalid, 1u);
		EXPECT_EQ(document.firstInvalidPath, "window.width");
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, WindowSideOutsideTheLimitsIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "window": { "width": 639, "height": 16385 } })") };

	EXPECT_EQ(document.settings.window, SWindowSettings{});
	EXPECT_EQ(document.numInvalid, 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, ZeroDetailsWidthIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "window": { "detailsWidth": 0 } })") };

	EXPECT_EQ(document.settings.window, SWindowSettings{});
	EXPECT_EQ(document.firstInvalidPath, "window.detailsWidth");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, WindowSidesAtTheLimitsAreAccepted)
{
	SSettingsDocument const document{ ReadValid(R"({ "window": { "width": 640, "height": 16384 } })") };

	EXPECT_EQ(document.settings.window.width, 640u);
	EXPECT_EQ(document.settings.window.height, 16384u);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, SectionOfWrongKindIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "window": 5 })") };

	EXPECT_EQ(document.settings.window, SWindowSettings{});
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "window");
}

//////////////////////////////////////////////////////////////////////////
// Its game may have been removed, so the file is not repaired over it.
TEST(SettingsJson, SelectedGameTheCatalogLacksGivesWayToTheFirstListed)
{
	std::span<Query::SGameDefinition const> const catalog{ Query::GetGameCatalog() };
	SSettingsDocument const document{ ReadValid(std::format(R"({{ "game": "doom", "games": {{ "{}": {{ "listed": false }} }} }})", catalog[0].key)) };

	ASSERT_GE(catalog.size(), 2u);
	EXPECT_EQ(document.settings.selectedGame, catalog[1].game);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, OrderPlaceOfAGameTheCatalogLacksIsKept)
{
	std::span<Query::SGameDefinition const> const catalog{ Query::GetGameCatalog() };
	std::string const kept{ std::format(R"({{ "gameOrder": [ "{}", "doom" ] }})", catalog[0].key) };
	nlohmann::ordered_json const written = nlohmann::ordered_json::parse(WriteSettingsJson(MakeDefaultSettings(), kept));

	ASSERT_GE(written["gameOrder"].size(), 2u);
	EXPECT_EQ(written["gameOrder"][1], "doom");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, NoListedGameListsEveryGame)
{
	SSettings settings{ MakeDefaultSettings() };

	for (SGameSettings& game : settings.games)
	{
		game.isListed = false;
	}

	SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

	EXPECT_EQ(document.settings, MakeDefaultSettings());
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "games");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, UnlistedSelectedGameGivesWayToTheFirstListed)
{
	std::span<Query::SGameDefinition const> const catalog{ Query::GetGameCatalog() };
	SSettings settings{ MakeDefaultSettings() };

	ASSERT_GE(catalog.size(), 3u);
	settings.selectedGame = catalog[1].game;
	settings.games[static_cast<size_t>(catalog[0].game)].isListed = false;
	settings.games[static_cast<size_t>(catalog[1].game)].isListed = false;

	SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

	EXPECT_EQ(document.settings.selectedGame, catalog[2].game);
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "game");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, UnlistedSelectedGameGivesWayToTheFirstListedInTheOrder)
{
	SSettings settings{ MakeDefaultSettings() };

	std::ranges::reverse(settings.gameOrder);
	settings.selectedGame = settings.gameOrder[0];
	settings.games[static_cast<size_t>(settings.gameOrder[0])].isListed = false;

	SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

	EXPECT_EQ(document.settings.selectedGame, settings.gameOrder[1]);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, OrderByNameWithoutAGameStaysByName)
{
	SSettings settings{ MakeDefaultSettings() };

	std::erase(settings.gameOrder, Fixtures::GetGameId("quake2"));

	SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

	EXPECT_EQ(document.settings.gameOrder, MakeDefaultSettings().gameOrder);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
// Reversed, the order starts with the game whose name sorts last, after Quake II's.
TEST(SettingsJson, GameMissingFromTheOrderGoesBeforeTheFirstThatSortsAfterIt)
{
	SSettings settings{ MakeDefaultSettings() };

	std::ranges::reverse(settings.gameOrder);
	std::erase(settings.gameOrder, Fixtures::GetGameId("quake2"));

	SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

	ASSERT_FALSE(document.settings.gameOrder.empty());
	EXPECT_EQ(document.settings.gameOrder.front(), Fixtures::GetGameId("quake2"));
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, UnknownGameInTheOrderIsSkipped)
{
	SSettingsDocument const document{ ReadValid(R"({ "gameOrder": [ "nosuchgame" ] })") };

	EXPECT_EQ(document.settings.gameOrder, MakeDefaultSettings().gameOrder);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, GameListedTwiceInTheOrderIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "gameOrder": [ "quake2", "quake2" ] })") };

	EXPECT_EQ(std::ranges::count(document.settings.gameOrder, Fixtures::GetGameId("quake2")), 1);
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "gameOrder[1]");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, OrderEntryThatIsNotAKeyIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "gameOrder": [ 5 ] })") };

	EXPECT_EQ(document.settings.gameOrder, MakeDefaultSettings().gameOrder);
	EXPECT_EQ(document.firstInvalidPath, "gameOrder[0]");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, OrderThatIsNotAListIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "gameOrder": "kingpin" })") };

	EXPECT_EQ(document.settings.gameOrder, MakeDefaultSettings().gameOrder);
	EXPECT_EQ(document.firstInvalidPath, "gameOrder");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, UnknownSortColumnIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "quake3": { "sort": { "column": "frags", "ascending": true } } } })") };
	SSortOrder const& sort{ document.settings.games[static_cast<size_t>(Fixtures::GetGameId("quake3"))].sort };

	EXPECT_EQ(sort.column, SSortOrder{}.column);
	EXPECT_TRUE(sort.isAscending);
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "games.quake3.sort.column");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, EverySortColumnRoundTrips)
{
	for (size_t index{ 0 }; index < NumSortColumns; ++index)
	{
		SCOPED_TRACE(index);

		SSettings settings{ MakeDefaultSettings() };

		settings.games[0].sort.column = static_cast<ESortColumn>(index);

		SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

		EXPECT_EQ(document.settings.games[0].sort.column, settings.games[0].sort.column);
		EXPECT_EQ(document.numInvalid, 0u);
	}
}

//////////////////////////////////////////////////////////////////////////
// Users' files hold these names, so renaming one would reset their sort.
TEST(SettingsJson, SortColumnNamesAreStable)
{
	constexpr std::array<std::pair<std::string_view, ESortColumn>, NumSortColumns> Names{ { { "name", ESortColumn::Name },
		{ "map", ESortColumn::Map }, { "mod", ESortColumn::Mod }, { "mode", ESortColumn::Mode }, { "players", ESortColumn::Players },
		{ "ping", ESortColumn::Ping }, { "favourite", ESortColumn::Favourite }, { "password", ESortColumn::Password },
		{ "country", ESortColumn::Country } } };

	for (auto const& [name, column] : Names)
	{
		SCOPED_TRACE(name);

		SSettingsDocument const document{ ReadValid(std::format(R"({{ "games": {{ "kingpin": {{ "sort": {{ "column": "{}" }} }} }} }})", name)) };

		EXPECT_EQ(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].sort.column, column);
		EXPECT_EQ(document.numInvalid, 0u);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, StringWithNulIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "filter": { "search": "run\u0000rm" } } } })") };

	EXPECT_TRUE(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].filter.search.empty());
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "games.kingpin.filter.search");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, MaxPingNullMeansNoLimit)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "filter": { "maxPing": null } } } })") };

	EXPECT_EQ(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].filter.maxPingMs, NoPingLimit);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, AutoRefreshOutOfRangeKeepsTheDefault)
{
	SSettingsDocument const document{ ReadValid(R"({ "autoRefreshSeconds": 86400 })") };

	EXPECT_EQ(document.settings.autoRefreshSeconds, MakeDefaultSettings().autoRefreshSeconds);
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "autoRefreshSeconds");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, AutoRefreshZeroMeansOff)
{
	SSettingsDocument const document{ ReadValid(R"({ "autoRefreshSeconds": 0 })") };

	EXPECT_EQ(document.settings.autoRefreshSeconds, 0u);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, BadFavouriteCostsOnlyItself)
{
	SSettingsDocument const document{ ReadValid(
		R"({ "games": { "kingpin": { "favourites": [ "203.0.113.7:31510", "nonsense", "203.0.113.8:27910" ] } } })") };
	std::vector<Query::SServerAddress> const expected{ Favourite, OtherFavourite };

	EXPECT_EQ(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].favourites, expected);
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "games.kingpin.favourites[1]");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, DuplicateFavouriteIsDropped)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "favourites": [ "203.0.113.7:31510", "203.0.113.7:31510" ] } } })") };
	std::vector<Query::SServerAddress> const expected{ Favourite };

	EXPECT_EQ(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].favourites, expected);
	EXPECT_EQ(document.numInvalid, 1u);
	EXPECT_EQ(document.firstInvalidPath, "games.kingpin.favourites[1]");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, CommentsAreAccepted)
{
	SSettingsDocument const document{ ReadValid("// edited by hand\n{ /* the one I play */ \"game\": \"quake2\" }") };

	EXPECT_EQ(document.settings.selectedGame, Fixtures::GetGameId("quake2"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsWithoutGamesTest, DefaultsSelectNoGame)
{
	EXPECT_EQ(MakeDefaultSettings().selectedGame, Query::NoGame);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsWithoutGamesTest, SavedGamesAreNotInvalid)
{
	std::expected<SSettingsDocument, ESettingsJsonError> const document{ ReadSettingsJson(R"({ "version": 2, "game": "kingpin",
		"games": { "kingpin": { "listed": false } }, "gameOrder": [ "kingpin" ] })") };

	ASSERT_TRUE(document.has_value());
	EXPECT_EQ(document->numInvalid, 0u);
	EXPECT_EQ(document->settings.selectedGame, Query::NoGame);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsWithoutGamesTest, SelectedGameIsKeptForItsReturn)
{
	std::string const text{ WriteSettingsJson(MakeDefaultSettings(), R"({ "version": 2, "game": "kingpin" })") };

	EXPECT_TRUE(text.contains("\"game\": \"kingpin\"")) << text;
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, NotJsonIsRejected)
{
	EXPECT_EQ(ReadSettingsJson("{ \"game\": ").error_or(ESettingsJsonError::NotAnObject), ESettingsJsonError::NotJson);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, NotJsonNamesWhereAfterAComment)
{
	std::string const error{ DescribeSettingsSyntaxError("// kept by hand\n{ \"game\": ") };

	EXPECT_TRUE(error.starts_with("line 2, column ")) << error;
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, RootThatIsNoObjectIsRejected)
{
	EXPECT_EQ(ReadSettingsJson("[ 1, 2 ]").error_or(ESettingsJsonError::NotJson), ESettingsJsonError::NotAnObject);
}

//////////////////////////////////////////////////////////////////////////
// Without exceptions a strict writer would abort here.
TEST(SettingsJson, InvalidUtf8IsWrittenAsReplacement)
{
	SSettings settings{ MakeDefaultSettings() };

	settings.games[0].filter.search = "run\xFF";

	SSettingsDocument const document{ ReadValid(WriteSettingsJson(settings)) };

	EXPECT_EQ(document.settings.games[0].filter.search, "run\xEF\xBF\xBD");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, BadInstallCostsOnlyItself)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "installs": [
		{ "id": 1, "command": "steam -applaunch 38430" },
		{ "command": "no id" },
		{ "id": 0, "command": "id zero" },
		{ "id": 2, "folder": "/games", "command": "both" },
		{ "id": 3, "name": "neither" },
		{ "id": 4, "folder": "" },
		{ "id": 1, "folder": "/games/repeated" },
		{ "id": 5, "name": "Native", "folder": "/opt/kingpin" }
	] } } })") };

	EXPECT_EQ(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].installs, (std::vector<SGameInstall>{
		SGameInstall{ 1, {}, EInstallKind::Command, "steam -applaunch 38430" },
		SGameInstall{ 5, "Native", EInstallKind::Folder, "/opt/kingpin" } }));
	EXPECT_EQ(document.numInvalid, 6u);
	EXPECT_EQ(document.firstInvalidPath, "games.kingpin.installs[1]");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, CustomCommandOfFormatOneBecomesAnInstall)
{
	SSettingsDocument const document{ ReadValid(R"({ "version": 1, "games": { "kingpin": { "launcher": "custom", "customCommand": "steam -applaunch 38430" } } })") };

	EXPECT_EQ(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].installs,
		(std::vector<SGameInstall>{ SGameInstall{ 1, {}, EInstallKind::Command, "steam -applaunch 38430" } }));
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, ObsoleteLauncherIsIgnored)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "launcher": "install:7" } } })") };

	EXPECT_EQ(document.settings, MakeDefaultSettings());
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, InstallWithANulIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "installs": [
		{ "id": 1, "name": "a\u0000b", "command": "run" },
		{ "id": 2, "command": "run\u0000rm" }
	] } } })") };

	EXPECT_TRUE(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].installs.empty());
	EXPECT_EQ(document.numInvalid, 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, EmptyCommandIsKept)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "installs": [ { "id": 1, "command": "" } ] } } })") };

	EXPECT_EQ(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].installs.size(), 1u);
	EXPECT_EQ(document.numInvalid, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, RelativeFolderIsRejected)
{
	SSettingsDocument const document{ ReadValid(R"({ "games": { "kingpin": { "installs": [ { "id": 1, "folder": "Games/Kingpin" } ] } } })") };

	EXPECT_TRUE(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].installs.empty());
	EXPECT_EQ(document.firstInvalidPath, "games.kingpin.installs[0]");
}

//////////////////////////////////////////////////////////////////////////
TEST(SettingsJson, EmptyCustomCommandOfFormatOneAddsNoInstall)
{
	SSettingsDocument const document{ ReadValid(R"({ "version": 1, "games": { "kingpin": { "launcher": "custom", "customCommand": "" } } })") };

	EXPECT_TRUE(document.settings.games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].installs.empty());
}
//////////////////////////////////////////////////////////////////////////
class CSettingsCatalogTest : public Fixtures::CCatalogTest
{
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CSettingsCatalogTest, RemovedGameFindsItsSettingsWhenItReturns)
{
	std::string const kept{ R"({ "games": { "doom": { "favourites": [ "203.0.113.7:31510" ] } } })" };
	std::string const written{ WriteSettingsJson(MakeDefaultSettings(), kept) };
	Query::SGameDefinition doom{ Fixtures::GetGameByKey("quake2") };

	doom.key = "doom";

	Query::EGame const game{ AddGame(std::move(doom)) };
	SSettingsDocument const document{ ReadValid(written) };

	EXPECT_EQ(document.settings.games[static_cast<size_t>(game)].favourites, std::vector<Query::SServerAddress>{ Favourite });
}
} // namespace
} // namespace Lkt::Config
