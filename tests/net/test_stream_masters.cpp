#include "catalog_fixture.hpp"
#include "fixtures.hpp"
#include "net/collect_events.hpp"
#include "net/event_collector.hpp"
#include "net/loopback_server.hpp"
#include "net/loopback_stream_server.hpp"
#include "net/query_engine.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;
using Fixtures::ToBytes;

constexpr std::chrono::milliseconds Patience{ 10s };
constexpr size_t OddWriteSize{ 3 };
constexpr size_t LargeWriteSize{ 64u << 10 };

//////////////////////////////////////////////////////////////////////////
// The frames of tests/scripts/tcp_list.lua: a kind, then an address.
void AppendFrame(std::vector<std::byte>& stream, char kind, Query::SServerAddress const& address)
{
	stream.emplace_back(static_cast<std::byte>(kind));

	for (int shift{ 24 }; shift >= 0; shift -= 8)
	{
		stream.emplace_back(static_cast<std::byte>(address.ipv4 >> shift));
	}

	stream.emplace_back(static_cast<std::byte>(address.port >> 8));
	stream.emplace_back(static_cast<std::byte>(address.port));
}

//////////////////////////////////////////////////////////////////////////
size_t CountOpenDescriptors()
{
	return static_cast<size_t>(std::ranges::distance(std::filesystem::directory_iterator{ "/proc/self/fd" }, std::filesystem::directory_iterator{}));
}

//////////////////////////////////////////////////////////////////////////
bool WaitForConnections(Fixtures::CLoopbackStreamServer const& master, uint32_t numConnections)
{
	auto const deadline{ std::chrono::steady_clock::now() + Patience };

	while (master.GetNumConnections() < numConnections && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(10ms);
	}

	return master.GetNumConnections() >= numConnections;
}

//////////////////////////////////////////////////////////////////////////
// A game whose master speaks tests/scripts/tcp_list.lua over TCP on loopback, listing a status server that answers.
class CStreamMasterTest : public Fixtures::CCatalogTest
{
protected:

	// A master that greets, takes the request, and sends these frames in odd-sized writes.
	Fixtures::SLoopbackStream MakeStream(std::vector<std::byte> reply, bool closesWhenDone = true) const
	{
		std::vector<Fixtures::SLoopbackStreamStep> steps{};

		steps.emplace_back(std::vector<std::byte>{}, ToBytes("HELLO"));
		steps.emplace_back(ToBytes("LIST"), std::move(reply));

		return Fixtures::SLoopbackStream{ std::move(steps), OddWriteSize, closesWhenDone };
	}

	std::vector<std::byte> ListStatusServer() const
	{
		std::vector<std::byte> frames{};

		AppendFrame(frames, 'S', m_status.GetAddress());

		return frames;
	}

	Query::EGame AddStreamGame(uint16_t masterPort)
	{
		Query::SGameDefinition game{ Fixtures::GetGameByKey("quake3") };

		game.key = "stream-master";
		game.protocol = AddProtocol("tcp_list");
		game.protocolOptions.clear();
		game.masters = { Query::SMasterEndpoint{ "127.0.0.1", masterPort } };

		return AddGame(std::move(game));
	}

	// One refresh of the game, run to its end.
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
		m_master.Stop();

		return isFinished;
	}

	std::vector<SMasterFailed> GetMasterFailures() const
	{
		return Fixtures::CollectEvents<SMasterFailed>(m_collector.GetEvents());
	}

	size_t CountAnswered() const
	{
		return Fixtures::CollectEvents<SServerAnswered>(m_collector.GetEvents()).size();
	}

	Fixtures::CLoopbackServer m_status;
	Fixtures::CLoopbackStreamServer m_master;
	Fixtures::CEventCollector m_collector;
	CQueryEngine m_engine;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CStreamMasterTest, MasterListsItsServers)
{
	ASSERT_TRUE(m_status.Start(ToBytes("hello")));

	std::vector<std::byte> frames{ ListStatusServer() };

	AppendFrame(frames, 'N', Query::SServerAddress{});
	AppendFrame(frames, 'E', Query::SServerAddress{});

	ASSERT_TRUE(m_master.Start(MakeStream(std::move(frames))));
	ASSERT_TRUE(Run(AddStreamGame(m_master.GetAddress().port)));
	EXPECT_TRUE(GetMasterFailures().empty());
	EXPECT_EQ(CountAnswered(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStreamMasterTest, ClosedStreamEndsTheList)
{
	ASSERT_TRUE(m_status.Start(ToBytes("hello")));
	ASSERT_TRUE(m_master.Start(MakeStream(ListStatusServer())));
	ASSERT_TRUE(Run(AddStreamGame(m_master.GetAddress().port)));
	EXPECT_TRUE(GetMasterFailures().empty());
	EXPECT_EQ(CountAnswered(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStreamMasterTest, MasterThatStaysOpenEndsByQuiet)
{
	ASSERT_TRUE(m_status.Start(ToBytes("hello")));
	ASSERT_TRUE(m_master.Start(MakeStream(ListStatusServer(), false)));
	ASSERT_TRUE(Run(AddStreamGame(m_master.GetAddress().port)));
	EXPECT_TRUE(GetMasterFailures().empty());
	EXPECT_EQ(CountAnswered(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStreamMasterTest, StreamClosedMidFrameIsCutShort)
{
	ASSERT_TRUE(m_status.Start(ToBytes("hello")));

	std::vector<std::byte> frames{ ListStatusServer() };

	frames.insert(frames.end(), { std::byte{ 'S' }, std::byte{ 1 }, std::byte{ 2 } });

	ASSERT_TRUE(m_master.Start(MakeStream(std::move(frames))));
	ASSERT_TRUE(Run(AddStreamGame(m_master.GetAddress().port)));

	std::vector<SMasterFailed> const failures{ GetMasterFailures() };

	ASSERT_EQ(failures.size(), 1u);
	EXPECT_EQ(failures[0].reason, "closed the connection before its list was complete");
	EXPECT_EQ(CountAnswered(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStreamMasterTest, RefusedMasterFails)
{
	ASSERT_TRUE(m_master.Start(MakeStream({})));

	uint16_t const closedPort{ m_master.GetAddress().port };

	m_master.Stop();

	ASSERT_TRUE(Run(AddStreamGame(closedPort)));

	std::vector<SMasterFailed> const failures{ GetMasterFailures() };

	ASSERT_EQ(failures.size(), 1u);
	EXPECT_EQ(failures[0].reason, "refused the query");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStreamMasterTest, SilentMasterFailsAfterItsStepWithoutReconnecting)
{
	ASSERT_TRUE(m_master.Start(Fixtures::SLoopbackStream{}));
	ASSERT_TRUE(Run(AddStreamGame(m_master.GetAddress().port)));

	std::vector<SMasterFailed> const failures{ GetMasterFailures() };

	ASSERT_EQ(failures.size(), 1u);
	EXPECT_EQ(failures[0].reason, "did not answer");
	EXPECT_EQ(m_master.GetNumConnections(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStreamMasterTest, StreamPastTheCapFails)
{
	std::vector<std::byte> noise{};

	for (size_t index{ 0 }; index < (5u << 20) / 7; ++index)
	{
		AppendFrame(noise, 'N', Query::SServerAddress{});
	}

	Fixtures::SLoopbackStream stream{ MakeStream(std::move(noise)) };

	stream.writeSize = LargeWriteSize;

	ASSERT_TRUE(m_master.Start(std::move(stream)));
	ASSERT_TRUE(Run(AddStreamGame(m_master.GetAddress().port)));

	std::vector<SMasterFailed> const failures{ GetMasterFailures() };

	ASSERT_EQ(failures.size(), 1u);
	EXPECT_EQ(failures[0].reason, "sent more than one master may");
}

//////////////////////////////////////////////////////////////////////////
// Connections still open when their refresh is replaced, cancelled, or the engine stops.
TEST_F(CStreamMasterTest, EndedMastersLeaveNoDescriptorOpen)
{
	ASSERT_TRUE(m_master.Start(Fixtures::SLoopbackStream{}));

	Query::EGame const game{ AddStreamGame(m_master.GetAddress().port) };
	size_t const numBefore{ CountOpenDescriptors() };

	ASSERT_TRUE(m_engine.Initialize(m_collector.MakeCallback()));

	m_engine.Refresh(game, {});
	EXPECT_TRUE(WaitForConnections(m_master, 1));
	m_engine.Refresh(game, {});
	EXPECT_TRUE(WaitForConnections(m_master, 2));
	m_engine.Cancel(game);
	m_engine.Refresh(game, {});
	EXPECT_TRUE(WaitForConnections(m_master, 3));
	m_engine.Terminate();

	EXPECT_EQ(CountOpenDescriptors(), numBefore);

	m_master.Stop();
}
} // namespace
} // namespace Lkt::Net
