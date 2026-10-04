#include "catalog_fixture.hpp"
#include "fixtures.hpp"
#include "net/collect_events.hpp"
#include "net/event_collector.hpp"
#include "net/loopback_server.hpp"
#include "net/loopback_stream_server.hpp"
#include "net/query_engine.hpp"
#include "net/ut2004_emulator.hpp"
#include "query/game_definition.hpp"
#include "query/master_endpoint.hpp"
#include "query/server_summary.hpp"
#include "query/status_reply.hpp"
#include "query/styled_text.hpp"
#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds Patience{ 10s };
constexpr uint16_t JoinPort{ 7777 };

//////////////////////////////////////////////////////////////////////////
// The built-in UT2004 game against emulated servers and a master on loopback.
class CUt2004Test : public Fixtures::CCatalogTest
{
protected:

	// The built-in game, listed by the emulated master alone.
	Query::EGame AddLoopbackGame(uint16_t masterPort)
	{
		Query::SGameDefinition game{ GetUt2004Game() };

		game.key = "ut2004-loopback";
		game.masters = { Query::SMasterEndpoint{ "127.0.0.1", masterPort } };

		return AddGame(std::move(game));
	}

	Query::SGameDefinition const& GetUt2004Game() const
	{
		return Fixtures::GetGameByKey("ut2004");
	}

	// The one reply a refresh of an emulated server got, or none.
	std::optional<Query::SStatusReply> Ask(Fixtures::SUt2004ServerSetup const& setup)
	{
		std::optional<Query::SStatusReply> reply{};

		EXPECT_TRUE(m_server.StartExchanges(Fixtures::MakeUt2004Server(setup)));
		EXPECT_TRUE(Run(GetUt2004Game().game, m_server.GetAddress()));

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
	Fixtures::CLoopbackStreamServer m_master;
	Fixtures::CEventCollector m_collector;
	CQueryEngine m_engine;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, ReplyHoldsInfoAndRules)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(Query::FindRule(*reply, "map"), Fixtures::Ut2004Map);
	EXPECT_EQ(Query::FindRule(*reply, "AdminName"), Fixtures::Ut2004AdminName);
	EXPECT_EQ(Query::FindRule(*reply, "MOTD").size(), Fixtures::Ut2004MotdLength);
	EXPECT_EQ(Query::FindRule(*reply, "Mutator"), "MutInstaGib");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, PlayersCarryTheirTeams)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 2u);
	EXPECT_EQ(reply->players[0].name, "Alice");
	EXPECT_EQ(reply->players[0].ping, 40u);
	ASSERT_EQ(reply->players[0].fields.size(), 1u);
	EXPECT_EQ(reply->players[0].fields[0].value, "Red");
	EXPECT_EQ(reply->players[1].name, Fixtures::Ut2004WidePlayer);
	EXPECT_EQ(reply->players[1].score, -3);
	ASSERT_EQ(reply->players[1].fields.size(), 1u);
	EXPECT_EQ(reply->players[1].fields[0].value, "Blue");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, NameColoursDecodeAsRgb)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());

	Query::SStyledText const name{ Query::DecodeText(GetUt2004Game().text, Query::FindRule(*reply, "hostname")) };

	EXPECT_EQ(name.plain, Fixtures::Ut2004PlainName);
	ASSERT_EQ(name.runs.size(), 2u);
	EXPECT_TRUE(name.runs[0].hasColor);
	EXPECT_EQ(name.runs[0].color.r, 0xFF);
	EXPECT_EQ(name.runs[0].color.g, 0x80);
	EXPECT_EQ(name.runs[1].color.b, 0xFF);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, ServerJoinsAtThePortItNames)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->joinPort, JoinPort);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, SummaryReadsTheGamesKeys)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ JoinPort }) };

	ASSERT_TRUE(reply.has_value());

	Query::SServerSummary const summary{ Query::Summarize(GetUt2004Game(), *reply) };

	EXPECT_EQ(summary.numPlayers, 2u);
	EXPECT_EQ(summary.maxPlayers, 16u);
	EXPECT_EQ(summary.map, Fixtures::Ut2004Map);
	EXPECT_FALSE(summary.hasPassword);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, PasswordIsSeen)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ .joinPort = JoinPort, .hasPassword = true }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_TRUE(Query::Summarize(GetUt2004Game(), *reply).hasPassword);
}

//////////////////////////////////////////////////////////////////////////
// Packets 300 ms apart take 1.5 s, past the 1 s a step waits, yet no command is sent twice and the reply is whole.
TEST_F(CUt2004Test, SlowAnswerIsAskedOnceAndEndsByQuiet)
{
	auto const start{ std::chrono::steady_clock::now() };
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ .joinPort = JoinPort, .packetInterval = 300ms }) };
	auto const elapsed{ std::chrono::steady_clock::now() - start };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->players.size(), 2u);
	EXPECT_EQ(m_server.GetNumMatches(Fixtures::Ut2004InfoExchange), 1u);
	EXPECT_EQ(m_server.GetNumMatches(Fixtures::Ut2004RulesExchange), 1u);
	EXPECT_EQ(m_server.GetNumMatches(Fixtures::Ut2004PlayersExchange), 1u);
	EXPECT_GE(elapsed, 1500ms);
	EXPECT_LT(elapsed, 5s);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, EmptyServerSendsNoPlayers)
{
	std::optional<Query::SStatusReply> const reply{ Ask(Fixtures::SUt2004ServerSetup{ .joinPort = JoinPort, .hasPlayers = false }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_TRUE(reply->players.empty());
	EXPECT_EQ(Query::FindRule(*reply, "Mutator"), "MutInstaGib");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUt2004Test, MasterListsItsServersEndToEnd)
{
	ASSERT_TRUE(m_server.StartExchanges(Fixtures::MakeUt2004Server(Fixtures::SUt2004ServerSetup{ JoinPort })));
	ASSERT_TRUE(m_secondServer.StartExchanges(Fixtures::MakeUt2004Server(Fixtures::SUt2004ServerSetup{ JoinPort })));

	std::array<Query::SServerAddress, 2> const servers{ m_server.GetAddress(), m_secondServer.GetAddress() };

	ASSERT_TRUE(m_master.Start(Fixtures::MakeUt2004Master(servers)));
	ASSERT_TRUE(Run(AddLoopbackGame(m_master.GetAddress().port), std::nullopt));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size(), 2u);
	EXPECT_EQ(m_master.GetNumConnections(), 1u);
}
//////////////////////////////////////////////////////////////////////////
// The script makes nothing of the first bytes, a frame still incomplete, yet the master has answered: no step timeout.
TEST_F(CUt2004Test, MasterPausingMidFrameIsStillAnswering)
{
	ASSERT_TRUE(m_server.StartExchanges(Fixtures::MakeUt2004Server(Fixtures::SUt2004ServerSetup{ JoinPort })));

	std::array<Query::SServerAddress, 1> const servers{ m_server.GetAddress() };

	ASSERT_TRUE(m_master.Start(Fixtures::MakeUt2004Master(servers, 4500ms)));
	ASSERT_TRUE(Run(AddLoopbackGame(m_master.GetAddress().port), std::nullopt));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size(), 1u);
}
} // namespace
} // namespace Lkt::Net
