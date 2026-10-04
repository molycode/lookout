#include "fixtures.hpp"
#include "query/game_definition.hpp"
#include "query/game_catalog.hpp"
#include "script/protocol_script.hpp"
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
SServerSummary SummarizeFixture(std::string_view key, std::string_view path)
{
	SGameDefinition const& definition{ Fixtures::GetGameByKey(key) };
	SProtocolDefinition const& protocol{ GetProtocol(definition.protocol) };
	Script::CProtocolScript script{};
	std::expected<void, std::string> const loaded{ script.Initialize(protocol.name, protocol.source) };
	std::expected<SStatusReply, EParseError> const reply{ loaded.has_value() ? script.ParseStatusReply(Fixtures::LoadFixture(path)) : std::unexpected{ EParseError::ScriptFailed } };

	script.Terminate();

	EXPECT_TRUE(reply.has_value()) << path;

	return reply.has_value() ? Summarize(definition, reply.value()) : SServerSummary{};
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, KingpinBagmanWinsOverDeathmatch)
{
	EXPECT_EQ(SummarizeFixture("kingpin", "kingpin/status-93.226.82.165_31510.bin").mode, "Bagman");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, KingpinCoopIsNotDeathmatch)
{
	EXPECT_EQ(SummarizeFixture("kingpin", "kingpin/status-45.94.58.60_31515.bin").mode, "Co-op");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, KingpinPrefersTheModsTitle)
{
	EXPECT_EQ(Summarize(Fixtures::GetGameByKey("kingpin"), MakeReply({ { "gamename", "Monkey Mod v2.1 beta" }, { "game", "monkey" } })).mod, "Monkey Mod v2.1 beta");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, ModFallsBackToTheNextKey)
{
	EXPECT_EQ(Summarize(Fixtures::GetGameByKey("kingpin"), MakeReply({ { "game", "coop" } })).mod, "coop");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, PasswordOnlyWhenTheFlagIsSet)
{
	SGameDefinition const& kingpin{ Fixtures::GetGameByKey("kingpin") };

	EXPECT_TRUE(Summarize(kingpin, MakeReply({ { "password", "1" } })).hasPassword);
	EXPECT_FALSE(Summarize(kingpin, MakeReply({ { "password", "0" } })).hasPassword);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, Quake2NeedpassIsABitmask)
{
	SGameDefinition const& quake2{ Fixtures::GetGameByKey("quake2") };

	EXPECT_TRUE(Summarize(quake2, MakeReply({ { "needpass", "3" } })).hasPassword);
	EXPECT_FALSE(Summarize(quake2, MakeReply({ { "needpass", "2" } })).hasPassword);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, MaxPlayersUnknownWithoutTheRule)
{
	EXPECT_EQ(Summarize(Fixtures::GetGameByKey("quake3"), MakeReply({ { "sv_hostname", "x" } })).maxPlayers, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, CountsThePlayers)
{
	EXPECT_EQ(SummarizeFixture("rtcw", RtcwFixture).numPlayers, 32u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, ReadsMaxPlayers)
{
	EXPECT_EQ(SummarizeFixture("rtcw", RtcwFixture).maxPlayers, 64u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, RtcwNameIsDecoded)
{
	SServerSummary const summary{ SummarizeFixture("rtcw", RtcwFixture) };

	EXPECT_GT(summary.name.runs.size(), 1u);
	EXPECT_EQ(summary.name.plain.find('^'), std::string::npos);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, RtcwObjectiveMode)
{
	EXPECT_EQ(SummarizeFixture("rtcw", RtcwFixture).mode, "Objective");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, UrbanTerrorIsForeignToQuake3)
{
	EXPECT_TRUE(SummarizeFixture("quake3", "quake3/status-152.70.60.136_27970.bin").isForeign);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerSummary, BaseQuake3IsNotForeign)
{
	EXPECT_FALSE(Summarize(Fixtures::GetGameByKey("quake3"), MakeReply({ { "gamename", "baseq3" } })).isForeign);
}
} // namespace
} // namespace Lkt::Query
