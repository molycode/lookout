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
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
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
std::vector<std::byte> MakeQuake2List(std::span<Query::SServerAddress const> servers)
{
	std::vector<std::byte> list{ ToBytes("\xFF\xFF\xFF\xFFservers\n") };

	for (Query::SServerAddress const& server : servers)
	{
		for (int shift{ 24 }; shift >= 0; shift -= 8)
		{
			list.emplace_back(static_cast<std::byte>(server.ipv4 >> shift));
		}

		list.emplace_back(static_cast<std::byte>(server.port >> 8));
		list.emplace_back(static_cast<std::byte>(server.port));
	}

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
		return m_status.Start(Fixtures::LoadFixture("kingpin-quake2/status-93.226.82.165_31510.bin"));
	}

	bool StartQuake2Master(std::vector<Query::SServerAddress> const& listed)
	{
		return m_quake2Master.Start(MakeQuake2List(listed));
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
		return RunWith(game, [this](Query::EGame refreshed) { m_engine.Refresh(refreshed, {}); });
	}

	bool Probe(Query::EGame game)
	{
		return RunWith(game, [this](Query::EGame probed) { m_engine.ProbeMasters(probed); });
	}

	bool RunWith(Query::EGame game, std::function<void(Query::EGame)> const& start)
	{
		bool isFinished{ false };

		if (m_engine.Initialize(Fixtures::GetCatalog(), m_collector.MakeCallback()))
		{
			start(game);
			isFinished = m_collector.WaitForFinish(m_engine, game, Patience);
			m_engine.Terminate();
		}

		m_status.Stop();
		m_quake2Master.Stop();
		m_gamespyMaster.Stop();

		return isFinished;
	}

	// Each master that answered, by index, with the number of servers it listed.
	std::vector<std::pair<size_t, uint32_t>> GetAnswered() const
	{
		std::vector<std::pair<size_t, uint32_t>> answered{};

		for (SMasterAnswered const& event : Fixtures::CollectEvents<SMasterAnswered>(m_collector.GetEvents()))
		{
			answered.emplace_back(event.index, event.numServers);
		}

		std::ranges::sort(answered);

		return answered;
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
	ASSERT_TRUE(StartQuake2Master({ m_status.GetAddress() }));
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

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, GameFileNamingAMastersProtocolListsThroughIt)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartGamespyMaster(GetGamespyAddress()));

	Query::SServerAddress const status{ m_status.GetAddress() };
	std::array<uint16_t, 1> const masterPorts{ m_gamespyMaster.GetAddress().port };

	ASSERT_TRUE(Run(AddGameFile("kingpin-gamespy", masterPorts)));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(GetListed(), std::vector<Query::SServerAddress>{ status });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, EachMasterSaysHowManyServersItListed)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartQuake2Master({ m_status.GetAddress() }));
	ASSERT_TRUE(StartGamespyMaster(GetGamespyAddress()));
	ASSERT_TRUE(Run(AddKingpin({ MakeQuake2Master(), MakeGamespyMaster() })));
	EXPECT_EQ(GetAnswered(), (std::vector<std::pair<size_t, uint32_t>>{ { 0, 1 }, { 1, 1 } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ServerAMasterListsTwiceIsCountedOnce)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartQuake2Master({ m_status.GetAddress(), m_status.GetAddress() }));
	ASSERT_TRUE(Run(AddKingpin({ MakeQuake2Master() })));
	EXPECT_EQ(GetAnswered(), (std::vector<std::pair<size_t, uint32_t>>{ { 0, 1 } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ListedPortShiftedOutOfRangeIsNotCounted)
{
	ASSERT_TRUE(StartGamespyMaster(Query::SServerAddress{ 0x7F000001, 65530 }));

	Query::EGame const game{ AddKingpin({ MakeGamespyMaster() }) };
	CExpectedLog const expected{ NetChannel, 1, 0 };

	ASSERT_TRUE(Run(game));
	EXPECT_EQ(GetAnswered(), (std::vector<std::pair<size_t, uint32_t>>{ { 0, 0 } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, FailedMasterSaysWhichItIs)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartQuake2Master({ m_status.GetAddress() }));
	ASSERT_TRUE(StartGamespyMaster(GetGamespyAddress()));

	Query::SMasterEndpoint const closed{ MakeGamespyMaster() };

	m_gamespyMaster.Stop();

	Query::EGame const game{ AddKingpin({ MakeQuake2Master(), closed }) };
	CExpectedLog const expected{ NetChannel, 1, 0 };

	ASSERT_TRUE(Run(game));

	std::vector<SMasterFailed> const failures{ Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()) };

	ASSERT_EQ(failures.size(), 1u);
	EXPECT_EQ(failures[0].index, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ProbeAsksNoServer)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartQuake2Master({ m_status.GetAddress() }));
	ASSERT_TRUE(Probe(AddKingpin({ MakeQuake2Master() })));
	EXPECT_EQ(m_status.GetNumRequests(), 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ProbeListsNoServer)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartQuake2Master({ m_status.GetAddress() }));
	ASSERT_TRUE(Probe(AddKingpin({ MakeQuake2Master() })));
	EXPECT_TRUE(GetListed().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProtocolTest, ProbeCountsWhatEachMasterLists)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartQuake2Master({ m_status.GetAddress() }));
	ASSERT_TRUE(StartGamespyMaster(GetGamespyAddress()));
	ASSERT_TRUE(Probe(AddKingpin({ MakeQuake2Master(), MakeGamespyMaster() })));
	EXPECT_EQ(GetAnswered(), (std::vector<std::pair<size_t, uint32_t>>{ { 0, 1 }, { 1, 1 } }));
}
} // namespace
} // namespace Lkt::Net
