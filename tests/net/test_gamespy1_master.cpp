#include "catalog_fixture.hpp"
#include "fixtures.hpp"
#include "net/collect_events.hpp"
#include "net/event_collector.hpp"
#include "net/gamespy1_emulator.hpp"
#include "net/loopback_server.hpp"
#include "net/loopback_stream_server.hpp"
#include "net/query_engine.hpp"
#include "query/game_definition.hpp"
#include "query/master_endpoint.hpp"
#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;
using Fixtures::ToBytes;

constexpr std::chrono::milliseconds Patience{ 10s };
constexpr std::string_view Status{ "\\hostname\\Deck\\mapname\\DM-Deck16][\\numplayers\\0\\maxplayers\\8\\hostport\\7777\\queryid\\1.1\\final\\" };

//////////////////////////////////////////////////////////////////////////
// lookout-games' Unreal Tournament against an emulated GameSpy master on loopback.
class CGamespy1MasterTest : public Fixtures::CCatalogTest
{
protected:

	Query::EGame AddLoopbackGame(uint16_t masterPort)
	{
		Query::SGameDefinition game{ Fixtures::GetGameByKey("ut99") };

		game.key = "ut99-loopback";
		game.masters = { Fixtures::MakeMaster(game, "127.0.0.1", masterPort) };

		return AddGame(std::move(game));
	}

	bool Run(Query::EGame game)
	{
		bool isFinished{ false };

		if (m_engine.Initialize(Fixtures::GetCatalog(), m_collector.MakeCallback()))
		{
			m_engine.Refresh(game, {});
			isFinished = m_collector.WaitForFinish(m_engine, game, Patience);
			m_engine.Terminate();
		}

		m_server.Stop();
		m_secondServer.Stop();
		m_master.Stop();

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

	Fixtures::CLoopbackServer m_server;
	Fixtures::CLoopbackServer m_secondServer;
	Fixtures::CLoopbackStreamServer m_master;
	Fixtures::CEventCollector m_collector;
	CQueryEngine m_engine;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1MasterTest, MasterListsItsServers)
{
	ASSERT_TRUE(m_server.Start(ToBytes(Status)));
	ASSERT_TRUE(m_secondServer.Start(ToBytes(Status)));

	std::array<Query::SServerAddress, 2> const servers{ m_server.GetAddress(), m_secondServer.GetAddress() };

	ASSERT_TRUE(m_master.Start(Fixtures::MakeGamespy1Master("ut", servers)));
	ASSERT_TRUE(Run(AddLoopbackGame(m_master.GetAddress().port)));
	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(GetListed(), (std::vector<Query::SServerAddress>{ servers.begin(), servers.end() }));
	EXPECT_EQ(Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size(), 2u);
}
} // namespace
} // namespace Lkt::Net
