#include "game_json.hpp"
#include "json/json.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

//////////////////////////////////////////////////////////////////////////
JsonValue MakeMinimalGame()
{
	return JsonValue::parse(R"json({
		"format": 1,
		"name": "Test Game",
		"protocol": "quake2",
		"textStyle": "ascii7",
		"masters": [ { "host": "master.example", "port": 27900 } ],
		"keys": { "hostname": "hostname", "map": "mapname", "maxPlayers": "maxclients", "password": "needpass" }
	})json");
}

//////////////////////////////////////////////////////////////////////////
// Empty when the game reads.
std::string ReadProblem(JsonValue const& game)
{
	std::expected<Query::SGameDefinition, std::string> const result{ ReadGameJson(game.dump()) };

	return result.has_value() ? std::string{} : result.error();
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, ReadsAMinimalGame)
{
	std::expected<Query::SGameDefinition, std::string> const game{ ReadGameJson(MakeMinimalGame().dump()) };

	ASSERT_TRUE(game.has_value()) << game.error();
	EXPECT_EQ(game->name, "Test Game");
	EXPECT_EQ(game->family, Query::EProtocolFamily::Quake2);
	ASSERT_EQ(game->masters.size(), 1u);
	EXPECT_EQ(game->masters.front().host, "master.example");
	EXPECT_EQ(game->masters.front().port, 27900);
	EXPECT_EQ(game->keys.password, "needpass");
	EXPECT_TRUE(game->modes.empty());
	EXPECT_TRUE(game->launch.program.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, AcceptsComments)
{
	std::string const text{ MakeMinimalGame().dump(1, '\t') };
	std::string const commented{ "// A game\n" + text };

	EXPECT_TRUE(ReadGameJson(commented).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, BuiltinGamesKeepTheirOrder)
{
	constexpr std::array<std::string_view, 5> Keys{ "kingpin", "quake2", "rtcw", "et", "quake3" };

	EXPECT_TRUE(std::ranges::equal(Query::GetGameCatalog(), Keys, {}, &Query::SGameDefinition::key));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, TextThatIsNotJsonIsRejected)
{
	EXPECT_FALSE(ReadGameJson("{ \"format\": 1,").has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, NewerFormatAsksForANewerLookout)
{
	JsonValue game = MakeMinimalGame();

	game["format"] = 2;

	EXPECT_EQ(ReadProblem(game), "format: is 2, which needs a newer Lookout");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, MissingNameIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game.erase("name");

	EXPECT_TRUE(ReadProblem(game).starts_with("name:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, UnknownProtocolIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["protocol"] = "a2s";

	EXPECT_TRUE(ReadProblem(game).starts_with("protocol:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, Quake3NeedsAMasterQuery)
{
	JsonValue game = MakeMinimalGame();

	game["protocol"] = "quake3";

	EXPECT_TRUE(ReadProblem(game).starts_with("masterQuery:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, Quake2RejectsAMasterQuery)
{
	JsonValue game = MakeMinimalGame();

	game["masterQuery"] = "68 empty full";

	EXPECT_TRUE(ReadProblem(game).starts_with("masterQuery:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, GameWithoutMastersIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["masters"] = JsonValue::array();

	EXPECT_TRUE(ReadProblem(game).starts_with("masters:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, PortOutsideItsRangeIsRejected)
{
	for (uint32_t const port : { 0u, 70000u })
	{
		JsonValue game = MakeMinimalGame();

		game["masters"][0]["port"] = port;

		EXPECT_TRUE(ReadProblem(game).starts_with("masters[0].port:")) << port;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, PartialLaunchIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["launch"] = JsonValue::parse(R"json({ "desktopFiles": [], "installDir": "Games/Test", "requiredFiles": [] })json");

	EXPECT_TRUE(ReadProblem(game).starts_with("launch.program:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, NulInAHostIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["masters"][0]["host"] = std::string{ "master.example\0evil", 19 };

	EXPECT_TRUE(ReadProblem(game).starts_with("masters[0].host:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, UnknownFieldIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["keys"]["maxplayers"] = "sv_maxclients";

	EXPECT_EQ(ReadProblem(game), "keys.maxplayers: is not a field of format 1");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, WrongTypeIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["name"] = 5;

	EXPECT_TRUE(ReadProblem(game).starts_with("name:"));
}
} // namespace
} // namespace Lkt::Games
