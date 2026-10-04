#include "catalog_fixture.hpp"
#include "net/a2s_emulator.hpp"
#include "net/collect_events.hpp"
#include "net/event_collector.hpp"
#include "net/loopback_server.hpp"
#include "net/query_engine.hpp"
#include "query/game_catalog.hpp"
#include "query/server_summary.hpp"
#include "query/status_reply.hpp"
#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds Patience{ 10s };
constexpr uint16_t JoinPort{ 27015 };

//////////////////////////////////////////////////////////////////////////
// The A2S test game, speaking tests/scripts/a2s.lua to emulated Source servers and a Steam master on loopback.
class CA2sTest : public Fixtures::CCatalogTest
{
protected:

	// testing::Test
	void SetUp() override
	{
		CCatalogTest::SetUp();
		AddProtocol("a2s");
	}
	// ~testing::Test

	Query::EGame AddA2sGame(uint16_t masterPort)
	{
		std::array<uint16_t, 1> const masterPorts{ masterPort };

		return AddGameFile("a2s", masterPorts);
	}

	// The one reply a refresh of that server got, or none.
	std::optional<Query::SStatusReply> Ask(Fixtures::SA2sServerSetup const& setup)
	{
		std::optional<Query::SStatusReply> reply{};
		Query::EGame const game{ AddA2sGame(0) };

		EXPECT_TRUE(m_server.StartExchanges(Fixtures::MakeA2sServer(setup)));
		EXPECT_TRUE(Run(game, m_server.GetAddress()));

		std::vector<SServerAnswered> const answered{ Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()) };

		EXPECT_EQ(answered.size(), 1u);

		if (answered.size() == 1)
		{
			reply = answered.front().reply;
		}

		return reply;
	}

	bool Run(Query::EGame game, std::optional<Query::SServerAddress> server)
	{
		bool isFinished{ false };

		if (m_engine.Initialize(m_collector.MakeCallback()))
		{
			if (server.has_value())
			{
				m_engine.RefreshServer(game, *server);
			}
			else
			{
				m_engine.Refresh(game, {});
			}

			isFinished = m_collector.WaitForFinish(m_engine, game, Patience);
			m_engine.Terminate();
		}

		m_server.Stop();
		m_secondServer.Stop();
		m_master.Stop();

		return isFinished;
	}

	Fixtures::CLoopbackServer m_server;
	Fixtures::CLoopbackServer m_secondServer;
	Fixtures::CLoopbackServer m_master;
	Fixtures::CEventCollector m_collector;
	CQueryEngine m_engine;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, ReplyHoldsInfoPlayersAndRules)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SA2sServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(Query::FindRule(*reply, "hostname"), Fixtures::A2sServerName);
	EXPECT_EQ(Query::FindRule(*reply, "keywords"), "alltalk,nocrits");
	EXPECT_EQ(Query::FindRule(*reply, Fixtures::A2sRule), Fixtures::A2sRuleValue);
	ASSERT_EQ(reply->players.size(), 2u);
	EXPECT_EQ(reply->players[0].name, "Alice");
	EXPECT_EQ(reply->players[1].score, -2);
	ASSERT_EQ(reply->players[0].fields.size(), 1u);
	EXPECT_EQ(reply->players[0].fields[0].key, "time");
	EXPECT_EQ(reply->players[0].fields[0].value, "123");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, ServerJoinsAtThePortItNames)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SA2sServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->joinPort, JoinPort);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, SummaryReadsTheGamesKeys)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SA2sServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());

	Query::SServerSummary const summary{ Query::Summarize(Query::GetGameCatalog().back(), *reply) };

	EXPECT_EQ(summary.numPlayers, 2u);
	EXPECT_EQ(summary.maxPlayers, 24u);
	EXPECT_EQ(summary.map, "cp_badlands");
	EXPECT_FALSE(summary.hasPassword);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, PasswordIsSeen)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SA2sServerSetup{ .joinPort = JoinPort, .hasPassword = true }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_TRUE(Query::Summarize(Query::GetGameCatalog().back(), *reply).hasPassword);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, LostQueryIsAskedAgain)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SA2sServerSetup{ .joinPort = JoinPort, .dropsFirstInfo = true }) };

	EXPECT_TRUE(reply.has_value());
	EXPECT_EQ(m_server.GetNumMatches(Fixtures::A2sChallengedInfoExchange), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, ServerThatAsksNoChallengeForInfoStillGetsOneForPlayers)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SA2sServerSetup{ .joinPort = JoinPort, .isChallenging = false }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->players.size(), 2u);
	EXPECT_EQ(m_server.GetNumMatches(Fixtures::A2sChallengedInfoExchange), 0u);
	EXPECT_EQ(m_server.GetNumMatches(Fixtures::A2sUnchallengedPlayersExchange), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, RulesThatNeverComeLeaveWhatCameBefore)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SA2sServerSetup{ .joinPort = JoinPort, .sendsRules = false }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(Query::FindRule(*reply, "hostname"), Fixtures::A2sServerName);
	EXPECT_EQ(reply->players.size(), 2u);
	EXPECT_TRUE(Query::FindRule(*reply, Fixtures::A2sRule).empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CA2sTest, SteamMasterPagesThroughItsList)
{
	ASSERT_TRUE(m_server.StartExchanges(Fixtures::MakeA2sServer(Fixtures::SA2sServerSetup{ JoinPort })));
	ASSERT_TRUE(m_secondServer.StartExchanges(Fixtures::MakeA2sServer(Fixtures::SA2sServerSetup{ JoinPort })));
	ASSERT_TRUE(m_master.StartExchanges(Fixtures::MakeSteamMaster(m_server.GetAddress(), m_secondServer.GetAddress())));

	Query::EGame const game{ AddA2sGame(m_master.GetAddress().port) };

	ASSERT_TRUE(Run(game, std::nullopt));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size(), 2u);
	EXPECT_EQ(m_master.GetNumMatches(Fixtures::SteamSecondPageExchange), 2u);
}
} // namespace
} // namespace Lkt::Net
