#include "catalog_fixture.hpp"
#include "fixtures.hpp"
#include "net/collect_events.hpp"
#include "net/event_collector.hpp"
#include "net/loopback_server.hpp"
#include "net/query_engine.hpp"
#include "net/socket_table.hpp"
#include "query/status_reply.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;
using Fixtures::ToBytes;

constexpr std::chrono::milliseconds Patience{ 10s };
constexpr std::string_view Loopback{ "127.0.0.1" };

//////////////////////////////////////////////////////////////////////////
// A Quake III master's list: each entry a backslash, then the address, then the end marker.
std::vector<std::byte> MakeMasterList(Query::SServerAddress const& server, size_t numEntries)
{
	std::vector<std::byte> list{ ToBytes("\xFF\xFF\xFF\xFFgetserversResponse") };

	for (size_t index{ 0 }; index < numEntries; ++index)
	{
		uint32_t const ip{ server.ipv4 };

		for (uint8_t const byte : { uint8_t{ '\\' }, static_cast<uint8_t>(ip >> 24), static_cast<uint8_t>(ip >> 16), static_cast<uint8_t>(ip >> 8),
			static_cast<uint8_t>(ip), static_cast<uint8_t>(server.port >> 8), static_cast<uint8_t>(server.port) })
		{
			list.emplace_back(static_cast<std::byte>(byte));
		}
	}

	std::ranges::copy(ToBytes("\\EOT"), std::back_inserter(list));

	return list;
}

//////////////////////////////////////////////////////////////////////////
bool WaitForRequests(Fixtures::CLoopbackServer const& server, uint32_t numRequests)
{
	auto const deadline{ std::chrono::steady_clock::now() + Patience };

	while (server.GetNumRequests() < numRequests && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(10ms);
	}

	return server.GetNumRequests() >= numRequests;
}

//////////////////////////////////////////////////////////////////////////
// Games added for one test: a copy of Quake III with its masters on loopback, or speaking a test script.
class CConversationTest : public Fixtures::CCatalogTest
{
protected:

	Query::EGame AddScriptGame(std::string_view key, std::string_view script)
	{
		Query::SGameDefinition game{ Fixtures::GetGameByKey("quake3") };

		game.key = key;
		game.protocol = AddProtocol(script);
		game.protocolOptions.clear();

		return AddGame(std::move(game));
	}

	Query::EGame AddMasterGame(std::string_view key, std::vector<uint16_t> const& masterPorts)
	{
		Query::SGameDefinition game{ Fixtures::GetGameByKey("quake3") };

		game.key = key;
		game.masters.clear();

		for (uint16_t const port : masterPorts)
		{
			game.masters.emplace_back(std::string{ Loopback }, port);
		}

		return AddGame(std::move(game));
	}

	bool StartStatusServer()
	{
		return m_status.Start(Fixtures::LoadFixture(Fixtures::ListFixtures("quake3", "status-").front().string()));
	}

	// Runs one refresh of the game to its end, or one server's when one is given.
	bool Run(Query::EGame game, std::optional<Query::SServerAddress> server = std::nullopt)
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

		m_status.Stop();
		m_master.Stop();

		return isFinished;
	}

	Fixtures::CLoopbackServer m_status;
	Fixtures::CLoopbackServer m_master;
	Fixtures::CEventCollector m_collector;
	CQueryEngine m_engine;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationTest, TwoStepConversationIsAnswered)
{
	Query::EGame const game{ AddScriptGame("two-step", "two_step") };

	ASSERT_TRUE(m_status.StartExchanges({ { ToBytes("token?"), { ToBytes("C42") } }, { ToBytes("status C42"), { ToBytes("hello") } } }));
	ASSERT_TRUE(Run(game, m_status.GetAddress()));

	std::vector<SServerAnswered> const answered{ Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()) };

	ASSERT_EQ(answered.size(), 1u);
	EXPECT_EQ(Query::FindRule(answered[0].reply, "hostname"), "hello");
	EXPECT_EQ(m_status.GetNumRequests(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationTest, ResendRepeatsEveryDatagramOfTheStep)
{
	Query::EGame const game{ AddScriptGame("counting", "counting") };

	ASSERT_TRUE(m_status.Start({}));
	ASSERT_TRUE(Run(game, m_status.GetAddress()));

	std::vector<SServerFailed> const failed{ Fixtures::CollectEvents<SServerFailed>(m_collector.GetEvents()) };

	ASSERT_EQ(failed.size(), 1u);
	EXPECT_EQ(failed[0].failure, EServerFailure::NoAnswer);
	EXPECT_EQ(m_status.GetNumRequests(), 4u);
}

//////////////////////////////////////////////////////////////////////////
// The script counts what it is given, so the count shows the datagram past the cap never reached it.
TEST_F(CConversationTest, ServerPastTheCapIsCutShortOnce)
{
	Query::EGame const game{ AddScriptGame("counting", "counting") };

	ASSERT_TRUE(m_status.StartExchanges({ { ToBytes("one"), std::vector<std::vector<std::byte>>(70, ToBytes("x")) }, { ToBytes("two"), {} } }));
	ASSERT_TRUE(Run(game, m_status.GetAddress()));

	std::vector<SServerAnswered> const answered{ Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()) };

	ASSERT_EQ(answered.size(), 1u);
	EXPECT_EQ(Query::FindRule(answered[0].reply, "count"), "64");
	EXPECT_TRUE(Fixtures::CollectEvents<SServerFailed>(m_collector.GetEvents()).empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationTest, MasterOnLoopbackListsItsServers)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(m_master.Start(MakeMasterList(m_status.GetAddress(), 1)));

	Query::SServerAddress const status{ m_status.GetAddress() };
	Query::EGame const game{ AddMasterGame("loopback-master", { m_master.GetAddress().port }) };

	ASSERT_TRUE(Run(game));

	std::vector<SServersListed> const listed{ Fixtures::CollectEvents<SServersListed>(m_collector.GetEvents()) };
	std::vector<SServerAnswered> const answered{ Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()) };

	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	ASSERT_EQ(listed.size(), 1u);
	EXPECT_EQ(listed[0].servers, std::vector<Query::SServerAddress>{ status });
	ASSERT_EQ(answered.size(), 1u);
	EXPECT_EQ(answered[0].address, status);
	EXPECT_EQ(m_master.GetNumRequests(), 1u);
}

//////////////////////////////////////////////////////////////////////////
// The master is stopped while its socket still holds a datagram, from inside its own read.
TEST_F(CConversationTest, MasterPastTheCapFails)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(m_master.StartExchanges({ { {}, std::vector<std::vector<std::byte>>(6, MakeMasterList(m_status.GetAddress(), 1000)) } }));

	Query::EGame const game{ AddMasterGame("loopback-master", { m_master.GetAddress().port }) };

	ASSERT_TRUE(Run(game));

	std::vector<SMasterFailed> const failed{ Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()) };

	ASSERT_EQ(failed.size(), 1u);
	EXPECT_EQ(failed[0].reason, "listed more servers than one master may; the rest were left out");
	EXPECT_EQ(Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationTest, RefusedMasterFailsAtOnce)
{
	ASSERT_TRUE(m_master.Start({}));

	uint16_t const closedPort{ m_master.GetAddress().port };

	m_master.Stop();

	Query::EGame const game{ AddMasterGame("refusing-master", { closedPort }) };
	auto const start{ std::chrono::steady_clock::now() };

	ASSERT_TRUE(Run(game));

	std::vector<SMasterFailed> const failed{ Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()) };

	ASSERT_EQ(failed.size(), 1u);
	EXPECT_EQ(failed[0].reason, "refused the query");
	EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationTest, SilentMasterIsAskedTwice)
{
	ASSERT_TRUE(m_master.Start({}));

	Query::EGame const game{ AddMasterGame("silent-master", { m_master.GetAddress().port }) };

	ASSERT_TRUE(Run(game));

	std::vector<SMasterFailed> const failed{ Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()) };

	ASSERT_EQ(failed.size(), 1u);
	EXPECT_EQ(failed[0].reason, "did not answer");
	EXPECT_EQ(m_master.GetNumRequests(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationTest, MastersAtOneAddressAreAskedApart)
{
	ASSERT_TRUE(StartStatusServer());
	ASSERT_TRUE(m_master.Start(MakeMasterList(m_status.GetAddress(), 1)));

	Query::EGame const game{ AddMasterGame("twin-masters", { m_master.GetAddress().port, m_master.GetAddress().port }) };

	ASSERT_TRUE(Run(game));

	EXPECT_TRUE(Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents()).empty());
	EXPECT_EQ(Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size(), 1u);
	EXPECT_EQ(m_master.GetNumRequests(), 2u);
}

//////////////////////////////////////////////////////////////////////////
// Masters still waiting when their refresh is replaced, cancelled, or the engine stops.
TEST_F(CConversationTest, EndedConversationsLeaveNoSocketOpen)
{
	ASSERT_TRUE(m_master.Start({}));

	Query::EGame const game{ AddMasterGame("silent-master", { m_master.GetAddress().port }) };

	ASSERT_TRUE(m_engine.Initialize(m_collector.MakeCallback()));

	m_engine.Refresh(game, {});
	EXPECT_TRUE(WaitForRequests(m_master, 1));
	m_engine.Refresh(game, {});
	EXPECT_TRUE(WaitForRequests(m_master, 2));
	EXPECT_EQ(Fixtures::CountSocketsTo(m_master.GetAddress()), 1u);
	m_engine.Cancel(game);
	m_engine.Refresh(game, {});
	EXPECT_TRUE(WaitForRequests(m_master, 3));
	m_engine.Terminate();

	EXPECT_EQ(Fixtures::CountSocketsTo(m_master.GetAddress()), 0u);

	m_master.Stop();
}
} // namespace
} // namespace Lkt::Net
