#include "catalog_fixture.hpp"
#include "channels.hpp"
#include "fixtures.hpp"
#include "net/collect_events.hpp"
#include "net/event_collector.hpp"
#include "net/gamespy1_emulator.hpp"
#include "net/loopback_server.hpp"
#include "net/loopback_stream_server.hpp"
#include "net/query_engine.hpp"
#include "query/game_definition.hpp"
#include "query/master_endpoint.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;
using Fixtures::NetChannel;
using Fixtures::ToBytes;
using Tge::Testing::CExpectedLog;

constexpr std::chrono::milliseconds Patience{ 10s };
// Kingpin's servers answer GameSpy's queries ten ports below the one they are joined and asked on.
constexpr int32_t GamespyPortOffset{ 10 };

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeQuake2List(Query::SServerAddress const& server)
{
	std::vector<std::byte> list{ ToBytes("\xFF\xFF\xFF\xFFservers\n") };

	for (int shift{ 24 }; shift >= 0; shift -= 8)
	{
		list.emplace_back(static_cast<std::byte>(server.ipv4 >> shift));
	}

	list.emplace_back(static_cast<std::byte>(server.port >> 8));
	list.emplace_back(static_cast<std::byte>(server.port));

	return list;
}

//////////////////////////////////////////////////////////////////////////
// lookout-games' Kingpin, whose servers speak quake2, with masters on loopback that list through quake2 or gamespy1.
class CMasterProtocolTest : public Fixtures::CCatalogTest
{
protected:

	Query::EGame AddKingpin(std::vector<Query::SMasterEndpoint> masters)
	{
		Query::SGameDefinition game{ Fixtures::GetGameByKey("kingpin") };

		game.key = "kingpin-loopback";
		game.masters = std::move(masters);

		return AddGame(std::move(game));
	}

	Query::SMasterEndpoint MakeQuake2Master() const
	{
		return Fixtures::MakeMaster(Fixtures::GetGameByKey("kingpin"), "127.0.0.1", m_quake2Master.GetAddress().port);
	}

	Query::SMasterEndpoint MakeGamespyMaster() const
	{
		Query::SMasterEndpoint master{};

		master.host = "127.0.0.1";
		master.port = m_gamespyMaster.GetAddress().port;
		master.protocol = Fixtures::GetProtocolId("gamespy1");
		master.protocolOptions = { { "masterGame", "kingpin" } };
		master.portOffset = GamespyPortOffset;

		return master;
	}

	bool StartStatusServer()
	{
		return m_status.Start(Fixtures::LoadFixture("kingpin/status-93.226.82.165_31510.bin"));
	}

	bool StartGamespyMaster(Query::SServerAddress const& listed)
	{
		std::array<Query::SServerAddress, 1> const servers{ listed };

		return m_gamespyMaster.Start(Fixtures::MakeGamespy1Master("kingpin", servers));
	}

	Query::SServerAddress GetGamespyAddress() const
	{
		return Query::SServerAddress{ m_status.GetAddress().ipv4, static_cast<uint16_t>(m_status.GetAddress().port - GamespyPortOffset) };
	}

	bool Run(Query::EGame game)
	{
		bool isFinished{ false };

		if (m_engine.Initialize(m_collector.MakeCallback()))
		{
			m_engine.Refresh(game, {});
			isFinished = m_collector.WaitForFinish(m_engine, game, Patience);
			m_engine.Terminate();
		}

		m_status.Stop();
		m_quake2Master.Stop();
		m_gamespyMaster.Stop();

		return isFinished;
	}

	std::vector<Query::SServerAddress> GetListed() const
	{
		std::vector<Query::SServerAddress> listed{};

		for (SServersListed const& event : Fixtures::CollectEvents<SServersListed>(m_collector.GetEvents()))
		{
			listed.insert(listed.end(), event.servers.begin(), event.servers.end());
		}

		return listed;
	}

	Fixtures::CLoopbackServer m_status;
	Fixtures::CLoopbackServer m_quake2Master;
	Fixtures::CLoopbackStreamServer m_gamespyMaster;
	Fixtures::CEventCollector m_collector;
	CQueryEngine m_engine;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, GamespyMasterListsAQuake2GamesServer)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartGamespyMaster(GetGamespyAddress()));

	Query::SServerAddress const status{ m_status.GetAddress() };

	ASSERT_TRUE(Run(AddKingpin({ MakeGamespyMaster() })));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(GetListed(), std::vector<Query::SServerAddress>{ status });
	EXPECT_EQ(Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ServerIsAskedThroughTheGamesProtocol)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartGamespyMaster(GetGamespyAddress()));
	ASSERT_TRUE(Run(AddKingpin({ MakeGamespyMaster() })));
	EXPECT_EQ(m_status.GetFirstRequest(), ToBytes("\xFF\xFF\xFF\xFFstatus\n"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ServerListedByBothMastersIsAskedOnce)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(m_quake2Master.Start(MakeQuake2List(m_status.GetAddress())));
	ASSERT_TRUE(StartGamespyMaster(GetGamespyAddress()));

	Query::SServerAddress const status{ m_status.GetAddress() };

	ASSERT_TRUE(Run(AddKingpin({ MakeQuake2Master(), MakeGamespyMaster() })));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(GetListed(), std::vector<Query::SServerAddress>{ status });
	EXPECT_EQ(m_status.GetNumRequests(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ListedPortShiftedOutOfRangeIsNotAsked)
{
	ASSERT_TRUE(StartGamespyMaster(Query::SServerAddress{ 0x7F000001, 65530 }));

	Query::EGame const game{ AddKingpin({ MakeGamespyMaster() }) };
	CExpectedLog const expected{ NetChannel, 1, 0 };

	ASSERT_TRUE(Run(game));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_TRUE(GetListed().empty());
}
} // namespace
} // namespace Lkt::Net
