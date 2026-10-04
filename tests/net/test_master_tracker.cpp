#include "master_tracker.hpp"
#include <gtest/gtest.h>
#include <array>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;

Clock::time_point const Start{ Clock::time_point{} + 1h };
constexpr Query::EGame Game{ 0 };
constexpr Query::EGame OtherGame{ 1 };
constexpr std::array<Query::SMasterEndpoint, 1> Masters{ Query::SMasterEndpoint{ "master.example", 27900 } };
constexpr std::array<Query::SMasterEndpoint, 2> TwoMasters{ Query::SMasterEndpoint{ "one.example", 27900 }, Query::SMasterEndpoint{ "two.example", 27900 } };
constexpr uint32_t MasterIp{ 0x2D5E3A3C };
constexpr uint32_t SecondMasterIp{ 0x2D5E3A3D };
constexpr Query::SServerAddress MasterAddress{ MasterIp, 27900 };

//////////////////////////////////////////////////////////////////////////
void BeginResolved(CMasterTracker& tracker)
{
	tracker.Begin(Game, 1, Masters, Start);
	tracker.OnResolved(Game, 1, 0, MasterIp, Start);
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterTracker, AsksTheMasterOnceResolved)
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
TEST(MasterTracker, AsksASilentMasterOnceMore)
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
TEST(MasterTracker, ReportsAMasterThatNeverAnswers)
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
TEST(MasterTracker, ReportsAMasterThatCannotBeResolved)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, Masters, Start);
	tracker.OnResolved(Game, 1, 0, std::unexpected{ std::string{ "Name or service not known" } }, Start);
	tracker.Update(Start, queries, outcomes);

	EXPECT_TRUE(queries.empty());
	ASSERT_EQ(outcomes.size(), 1u);
	EXPECT_EQ(outcomes[0].failure, "cannot be resolved: Name or service not known");
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterTracker, FinishesOnceTheMasterFallsQuiet)
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
TEST(MasterTracker, IgnoresAnAnswerForAReplacedRefresh)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, Masters, Start);
	tracker.Begin(Game, 2, Masters, Start);
	tracker.OnResolved(Game, 1, 0, MasterIp, Start);
	tracker.Update(Start, queries, outcomes);

	EXPECT_TRUE(queries.empty());
	EXPECT_TRUE(tracker.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterTracker, StrangerIsNotAMaster)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	BeginResolved(tracker);
	tracker.Update(Start, queries, outcomes);

	EXPECT_FALSE(tracker.OnDatagram(Query::SServerAddress{ MasterIp, 27901 }, Start).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterTracker, CapsTheServersOneMasterMayList)
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
TEST(MasterTracker, CancelOnlyForgetsThatGame)
{
	CMasterTracker tracker{};

	BeginResolved(tracker);
	tracker.Begin(OtherGame, 1, Masters, Start);
	tracker.Cancel(Game);

	EXPECT_FALSE(tracker.HasWork(Game));
	EXPECT_TRUE(tracker.HasWork(OtherGame));
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterTracker, ResolvesEachMasterByItsIndex)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, TwoMasters, Start);
	tracker.OnResolved(Game, 1, 1, SecondMasterIp, Start);
	tracker.Update(Start, queries, outcomes);

	ASSERT_EQ(queries.size(), 1u);
	EXPECT_EQ(queries[0].address, (Query::SServerAddress{ SecondMasterIp, 27900 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterTracker, AnswerStopsTheRetry)
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
TEST(MasterTracker, FinishedMasterTakesNoMoreDatagrams)
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
TEST(MasterTracker, LookupThatNeverReturnsTimesOut)
{
	CMasterTracker tracker{};
	std::vector<SMasterQuery> queries{};
	std::vector<SMasterOutcome> outcomes{};

	tracker.Begin(Game, 1, Masters, Start);
	tracker.Update(Start + 5s, queries, outcomes);

	ASSERT_EQ(outcomes.size(), 1u);
	EXPECT_EQ(outcomes[0].failure, "could not be resolved in time");
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterTracker, NextDeadlineFollowsTheQuery)
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
