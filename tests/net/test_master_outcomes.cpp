#include "catalog_fixture.hpp"
#include "channels.hpp"
#include "fixtures.hpp"
#include "net/collect_events.hpp"
#include "net/event_collector.hpp"
#include "net/loopback_server.hpp"
#include "net/query_engine.hpp"
#include "query/game_definition.hpp"
#include "query/master_endpoint.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <algorithm>
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
// lookout-games' Quake II, with masters on loopback.
class CMasterOutcomeTest : public Fixtures::CCatalogTest
{
protected:

	Query::EGame AddQuake2(std::vector<Query::SMasterEndpoint> masters)
	{
		Query::SGameDefinition game{ Fixtures::GetGameByKey("quake2") };

		game.key = "quake2-loopback";
		game.masters = std::move(masters);

		return AddGame(std::move(game));
	}

	static Query::SMasterEndpoint MakeMaster(Fixtures::CLoopbackServer const& master)
	{
		return Query::SMasterEndpoint{ "127.0.0.1", master.GetAddress().port };
	}

	bool StartStatusServer()
	{
		return m_status.Start(Fixtures::LoadFixture("kingpin-quake2/status-93.226.82.165_31510.bin"));
	}

	static bool StartMaster(Fixtures::CLoopbackServer& master, std::vector<Query::SServerAddress> const& listed)
	{
		return master.Start(MakeQuake2List(listed));
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
		m_firstMaster.Stop();
		m_secondMaster.Stop();

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
	Fixtures::CLoopbackServer m_firstMaster;
	Fixtures::CLoopbackServer m_secondMaster;
	Fixtures::CEventCollector m_collector;
	CQueryEngine m_engine;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterOutcomeTest, ServerListedByBothMastersIsAskedOnce)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartMaster(m_firstMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(StartMaster(m_secondMaster, { m_status.GetAddress() }));

	Query::SServerAddress const status{ m_status.GetAddress() };

	ASSERT_TRUE(Run(AddQuake2({ MakeMaster(m_firstMaster), MakeMaster(m_secondMaster) })));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(GetListed(), std::vector<Query::SServerAddress>{ status });
	EXPECT_EQ(m_status.GetNumRequests(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterOutcomeTest, EachMasterSaysHowManyServersItListed)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartMaster(m_firstMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(StartMaster(m_secondMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(Run(AddQuake2({ MakeMaster(m_firstMaster), MakeMaster(m_secondMaster) })));
	EXPECT_EQ(GetAnswered(), (std::vector<std::pair<size_t, uint32_t>>{ { 0, 1 }, { 1, 1 } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterOutcomeTest, ServerAMasterListsTwiceIsCountedOnce)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartMaster(m_firstMaster, { m_status.GetAddress(), m_status.GetAddress() }));
	ASSERT_TRUE(Run(AddQuake2({ MakeMaster(m_firstMaster) })));
	EXPECT_EQ(GetAnswered(), (std::vector<std::pair<size_t, uint32_t>>{ { 0, 1 } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterOutcomeTest, FailedMasterSaysWhichItIs)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartMaster(m_firstMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(StartMaster(m_secondMaster, {}));

	Query::SMasterEndpoint const closed{ MakeMaster(m_secondMaster) };

	m_secondMaster.Stop();

	Query::EGame const game{ AddQuake2({ MakeMaster(m_firstMaster), closed }) };
	CExpectedLog const expected{ NetChannel, 1, 0 };

	ASSERT_TRUE(Run(game));

	std::vector<SMasterFailed> const failures{ Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()) };

	ASSERT_EQ(failures.size(), 1u);
	EXPECT_EQ(failures[0].index, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterOutcomeTest, ProbeAsksNoServer)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartMaster(m_firstMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(Probe(AddQuake2({ MakeMaster(m_firstMaster) })));
	EXPECT_EQ(m_status.GetNumRequests(), 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterOutcomeTest, ProbeListsNoServer)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartMaster(m_firstMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(Probe(AddQuake2({ MakeMaster(m_firstMaster) })));
	EXPECT_TRUE(GetListed().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterOutcomeTest, ProbeCountsWhatEachMasterLists)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(StartMaster(m_firstMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(StartMaster(m_secondMaster, { m_status.GetAddress() }));
	ASSERT_TRUE(Probe(AddQuake2({ MakeMaster(m_firstMaster), MakeMaster(m_secondMaster) })));
	EXPECT_EQ(GetAnswered(), (std::vector<std::pair<size_t, uint32_t>>{ { 0, 1 }, { 1, 1 } }));
}
} // namespace
} // namespace Lkt::Net
