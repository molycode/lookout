#include "master_tracker.hpp"
#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;

Clock::time_point const Start{ Clock::time_point{} + 1h };
constexpr Query::EGame Game{ 0 };
constexpr Query::EGame OtherGame{ 1 };
constexpr SMasterId Master{ Game, 0 };
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

	void BeginResolved()
	{
		m_tracker.Begin(Game, 1, m_masters, Start);
		m_tracker.OnResolved(Game, 1, 0, MasterIp, Start);
	}

	// Asked at Start.
	void BeginAsked()
	{
		BeginResolved();
		m_tracker.Update(Start, m_queries, m_outcomes);
	}

	// A datagram the script made use of, with the quiet the Quake scripts ask for.
	void Answer(Clock::time_point when)
	{
		m_tracker.OnDatagram(Master, when);
		m_tracker.MarkStepAnswered(Master);
		m_tracker.SetQuiet(Master, 1500ms);
	}

	void Update(Clock::time_point when)
	{
		m_tracker.Update(when, m_queries, m_outcomes);
	}

	std::vector<Query::SMasterEndpoint> m_masters;
	std::vector<Query::SMasterEndpoint> m_twoMasters;
	CMasterTracker m_tracker;
	std::vector<SMasterQuery> m_queries;
	std::vector<SMasterOutcome> m_outcomes;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, AsksTheMasterOnceResolved)
{
	BeginAsked();

	ASSERT_EQ(m_queries.size(), 1u);
	EXPECT_EQ(m_queries[0].master, Master);
	EXPECT_EQ(m_queries[0].address, MasterAddress);
	EXPECT_TRUE(m_tracker.IsAsking(Master));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, AsksASilentMasterOnceMore)
{
	BeginAsked();
	Update(Start + 2s);

	EXPECT_EQ(m_queries.size(), 2u);
	EXPECT_TRUE(m_outcomes.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ReportsAMasterThatNeverAnswers)
{
	BeginAsked();
	Update(Start + 2s);
	Update(Start + 4s);

	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_EQ(m_outcomes[0].failure, "did not answer");
	EXPECT_FALSE(m_tracker.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ReportsAMasterThatCannotBeResolved)
{
	m_tracker.Begin(Game, 1, m_masters, Start);
	m_tracker.OnResolved(Game, 1, 0, std::unexpected{ std::string{ "Name or service not known" } }, Start);
	Update(Start);

	EXPECT_TRUE(m_queries.empty());
	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_EQ(m_outcomes[0].failure, "cannot be resolved: Name or service not known");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, LookupThatNeverReturnsTimesOut)
{
	m_tracker.Begin(Game, 1, m_masters, Start);
	Update(Start + 5s);

	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_EQ(m_outcomes[0].failure, "could not be resolved in time");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, IgnoresAnAnswerForAReplacedRefresh)
{
	m_tracker.Begin(Game, 1, m_masters, Start);
	m_tracker.Begin(Game, 2, m_masters, Start);
	m_tracker.OnResolved(Game, 1, 0, MasterIp, Start);
	Update(Start);

	EXPECT_TRUE(m_queries.empty());
	EXPECT_TRUE(m_tracker.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ResolvesEachMasterByItsIndex)
{
	m_tracker.Begin(Game, 1, m_twoMasters, Start);
	m_tracker.OnResolved(Game, 1, 1, SecondMasterIp, Start);
	Update(Start);

	ASSERT_EQ(m_queries.size(), 1u);
	EXPECT_EQ(m_queries[0].master, (SMasterId{ Game, 1 }));
	EXPECT_EQ(m_queries[0].address, (Query::SServerAddress{ SecondMasterIp, 27900 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, QuietCompletesTheList)
{
	BeginAsked();
	Answer(Start + 500ms);
	Update(Start + 1999ms);

	EXPECT_TRUE(m_outcomes.empty());

	Update(Start + 2000ms);

	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_TRUE(m_outcomes[0].failure.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, AnswerStopsTheRetry)
{
	BeginAsked();
	Answer(Start + 100ms);
	Answer(Start + 1s);
	Update(Start + 2s);

	EXPECT_EQ(m_queries.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
// A late reply to an earlier page must not stand in for the page the script asked for.
TEST_F(CMasterTrackerTest, DatagramTheScriptIgnoredDoesNotStopTheRetry)
{
	BeginAsked();
	m_tracker.OnDatagram(Master, Start + 100ms);
	Update(Start + 2s);

	EXPECT_EQ(m_queries.size(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, LostPageIsAskedForAgain)
{
	BeginAsked();
	Answer(Start + 100ms);
	m_tracker.BeginStep(Master, Start + 100ms);
	Update(Start + 2100ms);

	ASSERT_EQ(m_queries.size(), 2u);
	EXPECT_TRUE(m_outcomes.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ListCutShortIsAFailure)
{
	BeginAsked();
	Answer(Start + 100ms);
	m_tracker.AdmitEntries(Master, 231);
	m_tracker.BeginStep(Master, Start + 100ms);
	Update(Start + 2100ms);
	Update(Start + 4100ms);

	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_EQ(m_outcomes[0].failure, "stopped answering after listing 231 servers");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, DoneCompletesTheList)
{
	BeginAsked();
	Answer(Start + 100ms);
	m_tracker.Finish(Master, m_outcomes);

	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_TRUE(m_outcomes[0].failure.empty());
	EXPECT_FALSE(m_tracker.HasWork(Game));
	EXPECT_FALSE(m_tracker.IsAsking(Master));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, MasterThatNeverFallsQuietFailsAtTheDeadline)
{
	BeginAsked();

	for (Clock::time_point when{ Start + 1s }; when < Start + 30s; when += 1s)
	{
		Answer(when);
		Update(when);
	}

	EXPECT_TRUE(m_outcomes.empty());

	Update(Start + 30s);

	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_EQ(m_outcomes[0].failure, "did not finish its list in 30 s, after 0 servers");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, CapsTheServersOneMasterMayList)
{
	BeginAsked();
	Answer(Start);

	EXPECT_EQ(m_tracker.AdmitEntries(Master, 5000), 4096u);
	EXPECT_EQ(m_tracker.AdmitEntries(Master, 10), 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, MastersAtOneAddressAreApart)
{
	m_tracker.Begin(Game, 1, m_twoMasters, Start);
	m_tracker.OnResolved(Game, 1, 0, MasterIp, Start);
	m_tracker.OnResolved(Game, 1, 1, MasterIp, Start);
	Update(Start);
	m_tracker.OnDatagram(SMasterId{ Game, 0 }, Start + 100ms);
	m_tracker.MarkStepAnswered(SMasterId{ Game, 0 });
	Update(Start + 2s);

	ASSERT_EQ(m_queries.size(), 3u);
	EXPECT_EQ(m_queries[2].master, (SMasterId{ Game, 1 }));
	EXPECT_EQ(m_tracker.AdmitEntries(SMasterId{ Game, 0 }, 5000), 4096u);
	EXPECT_EQ(m_tracker.AdmitEntries(SMasterId{ Game, 1 }, 10), 10u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, FailureIsReportedAtOnce)
{
	BeginAsked();
	m_tracker.Fail(Master, "cannot be asked: master.start: broken", m_outcomes);

	ASSERT_EQ(m_outcomes.size(), 1u);
	EXPECT_EQ(m_outcomes[0].master, Master);
	EXPECT_EQ(m_outcomes[0].failure, "cannot be asked: master.start: broken");
	EXPECT_FALSE(m_tracker.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, EndedMasterIsNotReportedTwice)
{
	BeginAsked();
	m_tracker.Fail(Master, "cannot be asked: master.start: broken", m_outcomes);
	m_tracker.Finish(Master, m_outcomes);
	Update(Start + 5s);

	EXPECT_EQ(m_outcomes.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, CancelOnlyForgetsThatGame)
{
	BeginResolved();
	m_tracker.Begin(OtherGame, 1, m_masters, Start);
	m_tracker.Cancel(Game);

	EXPECT_FALSE(m_tracker.HasWork(Game));
	EXPECT_TRUE(m_tracker.HasWork(OtherGame));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, ResolvedMasterIsDueAtOnce)
{
	BeginResolved();

	EXPECT_EQ(m_tracker.GetNextDeadline(), std::optional<Clock::time_point>{ Start });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CMasterTrackerTest, NextDeadlineFollowsTheConversation)
{
	BeginAsked();

	EXPECT_EQ(m_tracker.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 2s });

	Answer(Start + 300ms);

	EXPECT_EQ(m_tracker.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 1800ms });
}
} // namespace
} // namespace Lkt::Net
