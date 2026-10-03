#include "fixtures.hpp"
#include "query/game_catalog.hpp"
#include "query/protocol.hpp"
#include "query/server_summary.hpp"
#include <gtest/gtest.h>
#include <initializer_list>

namespace Lkt::Query
{
namespace
{
constexpr std::string_view RtcwFixture{ "rtcw/status-104.153.105.209_27960.bin" };

//////////////////////////////////////////////////////////////////////////
SStatusReply MakeReply(std::initializer_list<SRule> rules)
{
	SStatusReply reply{};

	reply.rules.assign(rules);

	return reply;
}

//////////////////////////////////////////////////////////////////////////
SServerSummary SummarizeFixture(EGame game, std::string_view path)
{
	SGameDefinition const& definition{ GetGame(game) };
	std::expected<SStatusReply, EParseError> const reply{ GetProtocol(definition.family).ParseStatusReply(Fixtures::LoadFixture(path)) };

	EXPECT_TRUE(reply.has_value()) << path;

	return reply.has_value() ? Summarize(definition, reply.value()) : SServerSummary{};
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, KingpinBagmanWinsOverDeathmatch)
{
	EXPECT_EQ(SummarizeFixture(EGame::Kingpin, "kingpin/status-93.226.82.165_31510.bin").mode, "Bagman");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, KingpinCoopIsNotDeathmatch)
{
	EXPECT_EQ(SummarizeFixture(EGame::Kingpin, "kingpin/status-45.94.58.60_31515.bin").mode, "Co-op");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, KingpinPrefersTheModsTitle)
{
	EXPECT_EQ(Summarize(GetGame(EGame::Kingpin), MakeReply({ { "gamename", "Monkey Mod v2.1 beta" }, { "game", "monkey" } })).mod, "Monkey Mod v2.1 beta");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, ModFallsBackToTheNextKey)
{
	EXPECT_EQ(Summarize(GetGame(EGame::Kingpin), MakeReply({ { "game", "coop" } })).mod, "coop");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, PasswordOnlyWhenTheFlagIsSet)
{
	SGameDefinition const& kingpin{ GetGame(EGame::Kingpin) };

	EXPECT_TRUE(Summarize(kingpin, MakeReply({ { "password", "1" } })).hasPassword);
	EXPECT_FALSE(Summarize(kingpin, MakeReply({ { "password", "0" } })).hasPassword);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, Quake2NeedpassIsABitmask)
{
	SGameDefinition const& quake2{ GetGame(EGame::Quake2) };

	EXPECT_TRUE(Summarize(quake2, MakeReply({ { "needpass", "3" } })).hasPassword);
	EXPECT_FALSE(Summarize(quake2, MakeReply({ { "needpass", "2" } })).hasPassword);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, MaxPlayersUnknownWithoutTheRule)
{
	EXPECT_EQ(Summarize(GetGame(EGame::Quake3), MakeReply({ { "sv_hostname", "x" } })).maxPlayers, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, CountsThePlayers)
{
	EXPECT_EQ(SummarizeFixture(EGame::RtcwMultiplayer, RtcwFixture).numPlayers, 32u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, ReadsMaxPlayers)
{
	EXPECT_EQ(SummarizeFixture(EGame::RtcwMultiplayer, RtcwFixture).maxPlayers, 64u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, RtcwNameIsDecoded)
{
	SServerSummary const summary{ SummarizeFixture(EGame::RtcwMultiplayer, RtcwFixture) };

	EXPECT_GT(summary.name.runs.size(), 1u);
	EXPECT_EQ(summary.name.plain.find('^'), std::string::npos);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, RtcwObjectiveMode)
{
	EXPECT_EQ(SummarizeFixture(EGame::RtcwMultiplayer, RtcwFixture).mode, "Objective");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, UrbanTerrorIsForeignToQuake3)
{
	EXPECT_TRUE(SummarizeFixture(EGame::Quake3, "quake3/status-152.70.60.136_27970.bin").isForeign);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, BaseQuake3IsNotForeign)
{
	EXPECT_FALSE(Summarize(GetGame(EGame::Quake3), MakeReply({ { "gamename", "baseq3" } })).isForeign);
}
} // namespace
} // namespace Lkt::Query
