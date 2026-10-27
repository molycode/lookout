#include "channels.hpp"
#include "fixtures.hpp"
#include "net/loopback_server.hpp"
#include "net/loopback_stream.hpp"
#include "net/loopback_stream_server.hpp"
#include "net/socket_table.hpp"
#include "browser/master_probes.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/master_endpoint.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Browser
{
namespace
{
using namespace std::chrono_literals;
using Fixtures::NetChannel;
using Tge::Testing::CExpectedLog;

constexpr std::chrono::milliseconds Patience{ 10s };
constexpr Net::Clock::time_point Start{ std::chrono::hours{ 1 } };
constexpr std::chrono::seconds SettleTime{ 1 };
constexpr std::string_view Key{ "master" };
constexpr Query::SServerAddress Listed{ 0x7F000001, 27910 };

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> MakeQuake2List(Query::SServerAddress const& server)
{
	std::vector<std::byte> list{ Fixtures::ToBytes("\xFF\xFF\xFF\xFFservers\n") };

	for (int shift{ 24 }; shift >= 0; shift -= 8)
	{
		list.emplace_back(static_cast<std::byte>(server.ipv4 >> shift));
	}

	list.emplace_back(static_cast<std::byte>(server.port >> 8));
	list.emplace_back(static_cast<std::byte>(server.port));

	return list;
}

//////////////////////////////////////////////////////////////////////////
// Rows of lookout-games' Quake II whose masters are on loopback, tested against the whole catalog's protocols.
class CMasterProbesTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		m_probes.Initialize([this]()
		{
			{
				std::lock_guard const lock{ m_mutex };

				m_hasEvents = true;
			}

			m_wake.notify_one();
		});
	}

	void TearDown() override
	{
		m_probes.Terminate();
		m_master.Stop();
	}
	// ~testing::Test

	SMasterProbeRequest MakeRequest() const
	{
		Query::SGameDefinition const& quake2{ Fixtures::GetGameByKey("quake2") };

		return SMasterProbeRequest{ std::string{ Key }, Query::SMasterEndpoint{ "127.0.0.1", m_master.GetAddress().port }, quake2.protocol, quake2.protocolOptions };
	}

	void Want(std::vector<SMasterProbeRequest> const& requests, Net::Clock::time_point now, bool canStart = true)
	{
		m_probes.Want(requests, Query::GetProtocolCatalog(), canStart, now);
	}

	// Settled, so the row is due.
	void WantSettled(SMasterProbeRequest const& request)
	{
		Want({ request }, Start);
		Want({ request }, Start + SettleTime);
	}

	bool WaitUntilTested()
	{
		auto const deadline{ std::chrono::steady_clock::now() + Patience };
		bool isTested{ false };

		while (!isTested && std::chrono::steady_clock::now() < deadline)
		{
			{
				std::unique_lock lock{ m_mutex };

				m_wake.wait_until(lock, deadline, [this]() { return m_hasEvents; });
				m_hasEvents = false;
			}

			m_probes.Update();

			SMasterProbeResult const* const pResult{ m_probes.GetResult(Key) };

			isTested = pResult != nullptr && pResult->state != EProbeState::Probing;
		}

		return isTested;
	}

	CMasterProbes m_probes;
	Fixtures::CLoopbackServer m_master;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_hasEvents{ false };
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, RowIsNotTestedBeforeItSettles)
{
	ASSERT_TRUE(m_master.Start(MakeQuake2List(Listed)));
	Want({ MakeRequest() }, Start);
	EXPECT_EQ(m_probes.GetResult(Key), nullptr);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, SettledRowIsTested)
{
	ASSERT_TRUE(m_master.Start(MakeQuake2List(Listed)));
	WantSettled(MakeRequest());
	ASSERT_TRUE(WaitUntilTested());
	EXPECT_EQ(m_probes.GetResult(Key)->state, EProbeState::Answered);
	EXPECT_EQ(m_probes.GetResult(Key)->numServers, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, RowsWantedAfterAClearAreTestedAtOnce)
{
	ASSERT_TRUE(m_master.Start(MakeQuake2List(Listed)));
	m_probes.Clear();
	Want({ MakeRequest() }, Start);
	EXPECT_NE(m_probes.GetResult(Key), nullptr);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, FailedRowSaysWhy)
{
	Fixtures::CLoopbackStreamServer closed{};

	ASSERT_TRUE(closed.Start(Fixtures::SLoopbackStream{}));

	Query::SGameDefinition const& kingpin{ Fixtures::GetGameByKey("kingpin") };
	SMasterProbeRequest request{ std::string{ Key }, Query::SMasterEndpoint{ "127.0.0.1", closed.GetAddress().port }, kingpin.protocol,
		kingpin.protocolOptions };

	closed.Stop();

	CExpectedLog const expected{ NetChannel, 1, 0 };

	WantSettled(request);
	ASSERT_TRUE(WaitUntilTested());
	EXPECT_EQ(m_probes.GetResult(Key)->state, EProbeState::Failed);
	EXPECT_EQ(m_probes.GetResult(Key)->failure, "refused the query");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, ForgottenRowIsTestedAgain)
{
	ASSERT_TRUE(m_master.Start(MakeQuake2List(Listed)));
	WantSettled(MakeRequest());
	ASSERT_TRUE(WaitUntilTested());
	m_probes.Forget(Key);
	Want({ MakeRequest() }, Start + SettleTime);
	ASSERT_TRUE(WaitUntilTested());
	EXPECT_EQ(m_master.GetNumRequests(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, RowNoLongerWantedStopsItsTest)
{
	ASSERT_TRUE(m_master.Start({}));
	WantSettled(MakeRequest());
	ASSERT_NE(m_probes.GetResult(Key), nullptr);
	Want({}, Start + SettleTime);
	EXPECT_EQ(m_probes.GetResult(Key), nullptr);
	EXPECT_EQ(Fixtures::CountSocketsTo(m_master.GetAddress()), 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, NothingIsTestedWhileTheDescriptionCannotBe)
{
	ASSERT_TRUE(m_master.Start(MakeQuake2List(Listed)));
	Want({ MakeRequest() }, Start, false);
	Want({ MakeRequest() }, Start + SettleTime, false);
	EXPECT_EQ(m_probes.GetResult(Key), nullptr);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterProbesTest, NextDeadlineIsWhenARowSettles)
{
	ASSERT_TRUE(m_master.Start(MakeQuake2List(Listed)));
	Want({ MakeRequest() }, Start);
	EXPECT_EQ(m_probes.GetNextDeadline(), Start + SettleTime);
}

//////////////////////////////////////////////////////////////////////////
// The window would otherwise wake at once, over and over, until an edit made the description usable.
TEST_F(CMasterProbesTest, DueRowThatCannotBeTestedHasNoDeadline)
{
	ASSERT_TRUE(m_master.Start(MakeQuake2List(Listed)));
	Want({ MakeRequest() }, Start, false);
	Want({ MakeRequest() }, Start + SettleTime, false);
	EXPECT_FALSE(m_probes.GetNextDeadline().has_value());
}
} // namespace
} // namespace Lkt::Browser
