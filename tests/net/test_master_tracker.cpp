#include "master_tracker.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;

Clock::time_point const Start{ Clock::time_point{} + 1h };
constexpr Query::EGame Game{ 0 };
constexpr Query::EGame OtherGame{ 1 };
constexpr uint32_t MasterIp{ 0x2D5E3A3C };
constexpr uint32_t SecondMasterIp{ 0x2D5E3A3D };
constexpr Query::SServerAddress MasterAddress{ MasterIp, 27900 };

//////////////////////////////////////////////////////////////////////////
// The tracker keeps views of the master hosts, so the lists live as long as each test.
class CMasterTrackerTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		m_masters = { Query::SMasterEndpoint{ "master.example", 27900 } };
		m_twoMasters = { Query::SMasterEndpoint{ "one.example", 27900 }, Query::SMasterEndpoint{ "two.example", 27900 } };
	}
	// ~testing::Test

	void BeginResolved(CMasterTracker& tracker) const
	{
		tracker.Begin(Game, 1, m_masters, Start);
		tracker.OnResolved(Game, 1, 0, MasterIp, Start);
	}

	std::vector<Query::SMasterEndpoint> m_masters;
	std::vector<Query::SMasterEndpoint> m_twoMasters;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, AsksTheMasterOnceResolved)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);

	ASSERT_EQ(queries.size(), 1u);
	EXPECT_EQ(queries[0].address, MasterAddress);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, AsksASilentMasterOnceMore)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);
	tracker.Update(Start + 2s, queries, outcomes);

	EXPECT_EQ(queries.size(), 2u);
	EXPECT_TRUE(outcomes.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ReportsAMasterThatNeverAnswers)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);
	tracker.Update(Start + 2s, queries, outcomes);
	tracker.Update(Start + 4s, queries, outcomes);

	ASSERT_EQ(outcomes.size(), 1u);
	EXPECT_EQ(outcomes[0].failure, "did not answer");
	EXPECT_FALSE(tracker.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ReportsAMasterThatCannotBeResolved)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, m_masters, Start);
	tracker.OnResolved(Game, 1, 0, std::unexpected{ std::string{ "Name or service not known" } }, Start);
	tracker.Update(Start, queries, outcomes);

	EXPECT_TRUE(queries.empty());
	ASSERT_EQ(outcomes.size(), 1u);
	EXPECT_EQ(outcomes[0].failure, "cannot be resolved: Name or service not known");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, FinishesOnceTheMasterFallsQuiet)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);
	tracker.OnDatagram(MasterAddress, Start + 500ms);
	tracker.Update(Start + 1900ms, queries, outcomes);

	EXPECT_TRUE(outcomes.empty());

	tracker.Update(Start + 2000ms, queries, outcomes);

	ASSERT_EQ(outcomes.size(), 1u);
	EXPECT_TRUE(outcomes[0].failure.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, IgnoresAnAnswerForAReplacedRefresh)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, m_masters, Start);
	tracker.Begin(Game, 2, m_masters, Start);
	tracker.OnResolved(Game, 1, 0, MasterIp, Start);
	tracker.Update(Start, queries, outcomes);

	EXPECT_TRUE(queries.empty());
	EXPECT_TRUE(tracker.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, StrangerIsNotAMaster)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);

	EXPECT_FALSE(tracker.OnDatagram(Query::SServerAddress{ MasterIp, 27901 }, Start).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, CapsTheServersOneMasterMayList)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);
	tracker.OnDatagram(MasterAddress, Start);

	EXPECT_EQ(tracker.AdmitEntries(MasterAddress, 5000), 4096u);
	EXPECT_EQ(tracker.AdmitEntries(MasterAddress, 10), 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, CancelOnlyForgetsThatGame)
{
	CMasterTracker tracker{};

	BeginResolved(tracker);
	tracker.Begin(OtherGame, 1, m_masters, Start);
	tracker.Cancel(Game);

	EXPECT_FALSE(tracker.HasWork(Game));
	EXPECT_TRUE(tracker.HasWork(OtherGame));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ResolvesEachMasterByItsIndex)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, m_twoMasters, Start);
	tracker.OnResolved(Game, 1, 1, SecondMasterIp, Start);
	tracker.Update(Start, queries, outcomes);

	ASSERT_EQ(queries.size(), 1u);
	EXPECT_EQ(queries[0].address, (Query::SServerAddress{ SecondMasterIp, 27900 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, AnswerStopsTheRetry)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);
	tracker.OnDatagram(MasterAddress, Start + 100ms);
	tracker.Update(Start + 2s, queries, outcomes);

	EXPECT_EQ(queries.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, FinishedMasterTakesNoMoreDatagrams)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);
	tracker.OnDatagram(MasterAddress, Start);
	tracker.Update(Start + 2s, queries, outcomes);

	EXPECT_FALSE(tracker.OnDatagram(MasterAddress, Start + 3s).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, LookupThatNeverReturnsTimesOut)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, m_masters, Start);
	tracker.Update(Start + 5s, queries, outcomes);

	ASSERT_EQ(outcomes.size(), 1u);
	EXPECT_EQ(outcomes[0].failure, "could not be resolved in time");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, NextDeadlineFollowsTheQuery)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);

	EXPECT_EQ(tracker.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 2s });

	tracker.OnDatagram(MasterAddress, Start + 300ms);

	EXPECT_EQ(tracker.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 1800ms });
}
} // namespace
} // namespace Lkt::Net
