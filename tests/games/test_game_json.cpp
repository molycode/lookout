#include "game_json.hpp"
#include "json/json.hpp"
#include "query/game_catalog.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

//////////////////////////////////////////////////////////////////////////
// Stand-ins for loaded scripts: the reader needs only their names and declared options.
std::vector<Query::SProtocolDefinition> MakeProtocols()
{
	return { Query::SProtocolDefinition{ "quake2", {}, {} },
		Query::SProtocolDefinition{ "quake3", {}, { Query::SProtocolOption{ "masterQuery", "Words after getservers", true } } } };
}

//////////////////////////////////////////////////////////////////////////
JsonValue MakeMinimalGame()
{
	return JsonValue::parse(R"json({
		"format": 1,
		"name": "Test Game",
		"protocol": "quake2",
		"text": { "encoding": "ascii7" },
		"masters": [ { "host": "master.example", "port": 27900 } ],
		"keys": { "hostname": "hostname", "map": "mapname", "maxPlayers": "maxclients", "password": "needpass" },
		"join": { "arguments": [ "+connect", "{address}" ], "passwordArguments": [ "+password", "{password}", "+connect", "{address}" ],
			"password": { "maxLength": 63 } }
	})json");
}

//////////////////////////////////////////////////////////////////////////
// Empty when the game reads.
std::string ReadProblem(JsonValue const& game)
{
	std::expected<Query::SGameDefinition, std::string> const result{ ReadGameJson(game.dump(), MakeProtocols()) };

	return result.has_value() ? std::string{} : result.error();
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, ReadsAMinimalGame)
{
	std::expected<Query::SGameDefinition, std::string> const game{ ReadGameJson(MakeMinimalGame().dump(), MakeProtocols()) };

	ASSERT_TRUE(game.has_value()) << game.error();
	EXPECT_EQ(game->name, "Test Game");
	EXPECT_EQ(game->protocol, Query::EProtocol{ 0 });
	ASSERT_EQ(game->masters.size(), 1u);
	EXPECT_EQ(game->masters.front().host, "master.example");
	EXPECT_EQ(game->masters.front().port, 27900);
	EXPECT_EQ(game->queryPortOffset, 0);
	EXPECT_EQ(game->keys.password, "needpass");
	EXPECT_TRUE(game->modes.empty());
	EXPECT_TRUE(game->launch.program.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, AcceptsComments)
{
	std::string const text{ MakeMinimalGame().dump(1, '\t') };
	std::string const commented{ "// A game\n" + text };

	EXPECT_TRUE(ReadGameJson(commented, MakeProtocols()).has_value());
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
	EXPECT_FALSE(ReadGameJson("{ \"format\": 1,", MakeProtocols()).has_value());
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
TEST(GameJson, ProtocolOptionsAreRead)
{
	JsonValue game = MakeMinimalGame();

	game["protocol"] = "quake3";
	game["protocolOptions"]["masterQuery"] = "68 empty full";

	std::expected<Query::SGameDefinition, std::string> const read{ ReadGameJson(game.dump(), MakeProtocols()) };

	ASSERT_TRUE(read.has_value()) << read.error();
	EXPECT_EQ(read->protocol, Query::EProtocol{ 1 });
	EXPECT_EQ(read->protocolOptions.at("masterQuery"), "68 empty full");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, MissingRequiredOptionIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["protocol"] = "quake3";

	EXPECT_EQ(ReadProblem(game), "protocolOptions.masterQuery: is missing; the quake3 protocol requires it");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, UndeclaredOptionIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["protocolOptions"]["masterQuery"] = "68 empty full";

	EXPECT_EQ(ReadProblem(game), "protocolOptions.masterQuery: is not an option of the quake2 protocol");
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
//////////////////////////////////////////////////////////////////////////
TEST(GameJson, ColourCodesAreRead)
{
	JsonValue game = MakeMinimalGame();

	game["text"] = JsonValue::parse(R"json({ "encoding": "utf8OrWindows1252",
		"colourCodes": { "escape": "^", "codes": "printable", "palette": [ "#ff8000", "#000000" ] } })json");

	std::expected<Query::SGameDefinition, std::string> const read{ ReadGameJson(game.dump(), MakeProtocols()) };

	ASSERT_TRUE(read.has_value()) << read.error();
	EXPECT_EQ(read->text.codes, Query::EColorCodes::Printable);
	EXPECT_EQ(read->text.escape, '^');
	ASSERT_EQ(read->text.palette.size(), 2u);
	EXPECT_EQ(read->text.palette[0].r, 0xFF);
	EXPECT_EQ(read->text.palette[0].g, 0x80);
	EXPECT_EQ(read->text.palette[0].b, 0x00);
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, PaletteMustBeAPowerOfTwo)
{
	JsonValue game = MakeMinimalGame();

	game["text"] = JsonValue::parse(R"json({ "encoding": "ascii7",
		"colourCodes": { "escape": "^", "codes": "alphanumeric", "palette": [ "#000000", "#000000", "#000000" ] } })json");

	EXPECT_TRUE(ReadProblem(game).starts_with("text.colourCodes.palette:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, ColourMustBeHex)
{
	JsonValue game = MakeMinimalGame();

	game["text"] = JsonValue::parse(R"json({ "encoding": "ascii7",
		"colourCodes": { "escape": "^", "codes": "alphanumeric", "palette": [ "#00ff0g" ] } })json");

	EXPECT_TRUE(ReadProblem(game).starts_with("text.colourCodes.palette[0]:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, RgbCodesTakeNoPalette)
{
	JsonValue game = MakeMinimalGame();

	game["text"] = JsonValue::parse(R"json({ "encoding": "ascii7",
		"colourCodes": { "escape": "\u001b", "codes": "rgb", "palette": [ "#000000" ] } })json");

	EXPECT_TRUE(ReadProblem(game).starts_with("text.colourCodes.palette:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, EscapeMustBeOneCharacter)
{
	JsonValue game = MakeMinimalGame();

	game["text"] = JsonValue::parse(R"json({ "encoding": "ascii7", "colourCodes": { "escape": "^^", "codes": "rgb" } })json");

	EXPECT_TRUE(ReadProblem(game).starts_with("text.colourCodes.escape:"));
}
//////////////////////////////////////////////////////////////////////////
TEST(GameJson, UnknownPlaceholderIsRejected)
{
	JsonValue game = MakeMinimalGame();

	game["join"]["arguments"][1] = "{adress}";

	EXPECT_TRUE(ReadProblem(game).starts_with("join.arguments[1]:"));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, ArgumentsWithoutAddressAreRejected)
{
	JsonValue game = MakeMinimalGame();

	game["join"]["arguments"] = JsonValue::parse(R"json([ "+connect" ])json");

	EXPECT_EQ(ReadProblem(game), "join.arguments: must hold {address}");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, PasswordArgumentsNeedThePassword)
{
	JsonValue game = MakeMinimalGame();

	game["join"]["passwordArguments"] = JsonValue::parse(R"json([ "+connect", "{address}" ])json");

	EXPECT_EQ(ReadProblem(game), "join.passwordArguments: must hold {password}");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, ArgumentsWithoutAPasswordMustNotHoldOne)
{
	JsonValue game = MakeMinimalGame();

	game["join"]["arguments"] = JsonValue::parse(R"json([ "+password", "{password}", "+connect", "{address}" ])json");

	EXPECT_EQ(ReadProblem(game), "join.arguments: must not hold {password}");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, PasswordLengthIsRequired)
{
	JsonValue game = MakeMinimalGame();

	game["join"]["password"] = JsonValue::object();

	EXPECT_TRUE(ReadProblem(game).starts_with("join.password.maxLength:"));
}
//////////////////////////////////////////////////////////////////////////
TEST(GameJson, PlayerCountKeyIsRead)
{
	JsonValue game = MakeMinimalGame();

	game["keys"]["numPlayers"] = "clients";

	std::expected<Query::SGameDefinition, std::string> const read{ ReadGameJson(game.dump(), MakeProtocols()) };

	ASSERT_TRUE(read.has_value()) << read.error();
	EXPECT_EQ(read->keys.numPlayers, "clients");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, QueryPortOffsetIsRead)
{
	for (int32_t const offset : { 1, -10 })
	{
		JsonValue game = MakeMinimalGame();

		game["queryPortOffset"] = offset;

		std::expected<Query::SGameDefinition, std::string> const read{ ReadGameJson(game.dump(), MakeProtocols()) };

		ASSERT_TRUE(read.has_value()) << read.error();
		EXPECT_EQ(read->queryPortOffset, offset);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(GameJson, QueryPortOffsetOutsideItsRangeIsRejected)
{
	for (JsonValue const& offset : JsonValue::parse(R"json([ 65535, -65535, 18446744073709551615, 1.5, "1" ])json"))
	{
		JsonValue game = MakeMinimalGame();

		game["queryPortOffset"] = offset;

		EXPECT_TRUE(ReadProblem(game).starts_with("queryPortOffset:")) << offset.dump();
	}
}
} // namespace
} // namespace Lkt::Games
