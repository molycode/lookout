#include "request_scheduler.hpp"
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

//////////////////////////////////////////////////////////////////////////
SServerRequest MakeRequest(uint32_t index, Query::EGame game = Game)
{
	return SServerRequest{ game, Query::SServerAddress{ 0x2D000000 + index, 27960 } };
}

//////////////////////////////////////////////////////////////////////////
// One request sent at Start; what the clock did with it is left in due and ended.
class CConversationClockTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		m_scheduler.Add(MakeRequest(1));
		m_scheduler.TakeDue(Start, m_due);
		m_due.clear();
	}
	// ~testing::Test

	void Answer(Clock::time_point when, size_t size = 100)
	{
		m_scheduler.OnDatagram(MakeRequest(1).address, size, when);
		m_scheduler.MarkStepAnswered(MakeRequest(1));
	}

	void Tick(Clock::time_point when)
	{
		m_scheduler.TakeDue(when, m_due);
		m_scheduler.TakeEnded(when, m_ended);
	}

	CRequestScheduler m_scheduler;
	std::vector<SServerRequest> m_due;
	std::vector<SEndedRequest> m_ended;
};

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, SendsAnAddedRequest)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);

	ASSERT_EQ(due.size(), 1u);
	EXPECT_EQ(due[0].numAttempts, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, LimitsABurst)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	for (uint32_t index{ 0 }; index < 20; ++index)
	{
		scheduler.Add(MakeRequest(index));
	}

	scheduler.TakeDue(Start, due);

	EXPECT_EQ(due.size(), 9u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, LimitsRequestsInFlight)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	for (uint32_t index{ 0 }; index < 300; ++index)
	{
		scheduler.Add(MakeRequest(index));
	}

	for (uint32_t step{ 0 }; step < 45; ++step)
	{
		scheduler.TakeDue(Start + step * 20ms, due);
	}

	EXPECT_EQ(due.size(), 256u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, QueuedIsPerGame)
{
	CRequestScheduler scheduler{};

	scheduler.Add(MakeRequest(1, OtherGame));

	EXPECT_FALSE(scheduler.IsQueued(Game, MakeRequest(1).address));
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, SustainsFiveHundredRequestsASecond)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	for (uint32_t index{ 0 }; index < 2000; ++index)
	{
		scheduler.Add(MakeRequest(index));
	}

	for (uint32_t millisecond{ 0 }; millisecond < 1000; ++millisecond)
	{
		size_t const numBefore{ due.size() };

		scheduler.TakeDue(Start + millisecond * 1ms, due);

		for (size_t index{ numBefore }; index < due.size(); ++index)
		{
			scheduler.Remove(due[index]);
		}
	}

	EXPECT_GE(due.size(), 490u);
	EXPECT_LE(due.size(), 510u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, PacedRequestWaitsForItsSendTime)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	for (uint32_t index{ 0 }; index < 20; ++index)
	{
		scheduler.Add(MakeRequest(index));
	}

	scheduler.TakeDue(Start, due);

	EXPECT_EQ(scheduler.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 2ms });
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, FullWindowWaitsForATimeout)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	for (uint32_t index{ 0 }; index < 300; ++index)
	{
		scheduler.Add(MakeRequest(index));
	}

	for (uint32_t step{ 0 }; step < 45; ++step)
	{
		scheduler.TakeDue(Start + step * 20ms, due);
	}

	EXPECT_EQ(scheduler.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 1s });
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, ResendsShareThePace)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	for (uint32_t index{ 0 }; index < 40; ++index)
	{
		scheduler.Add(MakeRequest(index));
	}

	for (uint32_t step{ 0 }; step < 40; ++step)
	{
		scheduler.TakeDue(Start + step * 2ms, due);
	}

	ASSERT_EQ(due.size(), 40u);
	due.clear();
	scheduler.TakeDue(Start + 2s, due);

	EXPECT_EQ(due.size(), 9u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, DatagramFromAnUnaskedServerIsIgnored)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);

	EXPECT_FALSE(scheduler.OnDatagram(MakeRequest(2).address, 100, Start + 80ms).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, CancelDropsRequestsInFlight)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);
	scheduler.Cancel(Game);

	EXPECT_FALSE(scheduler.HasWork(Game));
	EXPECT_FALSE(scheduler.OnDatagram(MakeRequest(1).address, 100, Start + 10ms).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, CancelOnlyDropsThatGame)
{
	CRequestScheduler scheduler{};

	scheduler.Add(MakeRequest(1, Game));
	scheduler.Add(MakeRequest(2, OtherGame));
	scheduler.Cancel(Game);

	EXPECT_FALSE(scheduler.HasWork(Game));
	EXPECT_TRUE(scheduler.HasWork(OtherGame));
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, RemovedRequestNeverEnds)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};
	std::vector<SEndedRequest> ended{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);
	scheduler.Remove(due[0]);
	scheduler.TakeEnded(Start + 10s, ended);
	scheduler.TakeDue(Start + 10s, due);

	EXPECT_TRUE(ended.empty());
	EXPECT_EQ(due.size(), 1u);
	EXPECT_FALSE(scheduler.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, RemoveOnlyDropsThatGamesRequest)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};
	Query::SServerAddress const address{ MakeRequest(1).address };

	scheduler.Add(SServerRequest{ Game, address });
	scheduler.Add(SServerRequest{ OtherGame, address });
	scheduler.TakeDue(Start, due);
	scheduler.Remove(SServerRequest{ Game, address });

	EXPECT_FALSE(scheduler.HasWork(Game));
	EXPECT_TRUE(scheduler.HasWork(OtherGame));
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, AddressIsLeasedToOneConversation)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};
	Query::SServerAddress const address{ MakeRequest(1).address };

	scheduler.Add(SServerRequest{ Game, address });
	scheduler.Add(SServerRequest{ OtherGame, address });
	scheduler.TakeDue(Start, due);

	ASSERT_EQ(due.size(), 1u);
	EXPECT_EQ(due[0].game, Game);
	EXPECT_EQ(scheduler.OnDatagram(address, 100, Start + 10ms)->request.game, Game);

	scheduler.Remove(due[0]);
	scheduler.TakeDue(Start + 20ms, due);

	ASSERT_EQ(due.size(), 2u);
	EXPECT_EQ(due[1].game, OtherGame);
	EXPECT_EQ(scheduler.OnDatagram(address, 100, Start + 30ms)->request.game, OtherGame);
}

//////////////////////////////////////////////////////////////////////////
// The lease ends with an update anyway; a deadline of its own would wake the loop over and over until then.
TEST(RequestScheduler, LeasedRequestWaitsForNoDeadline)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};
	Query::SServerAddress const address{ MakeRequest(1).address };

	scheduler.Add(SServerRequest{ Game, address });
	scheduler.Add(SServerRequest{ OtherGame, address });
	scheduler.TakeDue(Start, due);

	EXPECT_EQ(scheduler.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 1s });
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, IdleSchedulerHasNoDeadline)
{
	EXPECT_FALSE(CRequestScheduler{}.GetNextDeadline().has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, KnowsWhatIsQueued)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
	scheduler.Add(MakeRequest(2));
	scheduler.TakeDue(Start, due);
	scheduler.Remove(MakeRequest(2));

	EXPECT_TRUE(scheduler.IsQueued(Game, MakeRequest(1).address));
	EXPECT_FALSE(scheduler.IsQueued(Game, MakeRequest(2).address));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, NextDeadlineIsTheTimeout)
{
	EXPECT_EQ(m_scheduler.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 1s });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, DatagramReportsTheRoundTripOfTheFirst)
{
	std::optional<SAnsweredRequest> const first{ m_scheduler.OnDatagram(MakeRequest(1).address, 100, Start + 80ms) };
	std::optional<SAnsweredRequest> const second{ m_scheduler.OnDatagram(MakeRequest(1).address, 100, Start + 300ms) };

	ASSERT_TRUE(first.has_value() && second.has_value());
	EXPECT_EQ(first->roundTrip, 80ms);
	EXPECT_EQ(second->roundTrip, 80ms);
	EXPECT_TRUE(m_scheduler.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, ResendsOnceAfterATimeout)
{
	Tick(Start + 1s);

	ASSERT_EQ(m_due.size(), 1u);
	EXPECT_EQ(m_due[0].numAttempts, 2u);
	EXPECT_TRUE(m_ended.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, NotResentJustBeforeTheTimeout)
{
	Tick(Start + 999ms);

	EXPECT_TRUE(m_due.empty());
	EXPECT_TRUE(m_ended.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, SilentServerEndsUnansweredAfterTheSecondTimeout)
{
	Tick(Start + 1s);
	Tick(Start + 2s);

	ASSERT_EQ(m_ended.size(), 1u);
	EXPECT_FALSE(m_ended[0].hasAnswered);
	EXPECT_FALSE(m_scheduler.HasWork(Game));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, AnsweredStepIsNotResent)
{
	Answer(Start + 100ms);
	Tick(Start + 1s);

	EXPECT_TRUE(m_due.empty());
	EXPECT_TRUE(m_ended.empty());
}

//////////////////////////////////////////////////////////////////////////
// A late reply to an earlier step must not stand in for the answer the script is waiting for.
TEST_F(CConversationClockTest, DatagramTheScriptIgnoredDoesNotStopTheResend)
{
	m_scheduler.OnDatagram(MakeRequest(1).address, 100, Start + 100ms);
	Tick(Start + 1s);

	EXPECT_EQ(m_due.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, NewStepIsResentFromItsOwnSend)
{
	Answer(Start + 100ms);
	m_scheduler.BeginStep(MakeRequest(1), Start + 100ms);
	Tick(Start + 1s);

	EXPECT_TRUE(m_due.empty());

	Tick(Start + 1100ms);

	ASSERT_EQ(m_due.size(), 1u);
	EXPECT_EQ(m_due[0].numAttempts, 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, UnansweredLaterStepEndsAnswered)
{
	Answer(Start + 100ms);
	m_scheduler.BeginStep(MakeRequest(1), Start + 100ms);
	Tick(Start + 1100ms);
	Tick(Start + 2100ms);

	ASSERT_EQ(m_ended.size(), 1u);
	EXPECT_TRUE(m_ended[0].hasAnswered);
	EXPECT_EQ(m_ended[0].roundTrip, 100ms);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, QuietEndsTheConversation)
{
	Answer(Start + 100ms);
	m_scheduler.SetQuiet(MakeRequest(1), 300ms);
	Tick(Start + 399ms);

	EXPECT_TRUE(m_ended.empty());

	Tick(Start + 400ms);

	ASSERT_EQ(m_ended.size(), 1u);
	EXPECT_TRUE(m_ended[0].hasAnswered);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, DatagramRestartsTheQuiet)
{
	Answer(Start + 100ms);
	m_scheduler.SetQuiet(MakeRequest(1), 300ms);
	Answer(Start + 300ms);
	Tick(Start + 500ms);

	EXPECT_TRUE(m_ended.empty());
	EXPECT_EQ(m_scheduler.GetNextDeadline(), std::optional<Clock::time_point>{ Start + 600ms });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, NewStepCancelsTheQuiet)
{
	Answer(Start + 100ms);
	m_scheduler.SetQuiet(MakeRequest(1), 300ms);
	m_scheduler.BeginStep(MakeRequest(1), Start + 200ms);
	Tick(Start + 500ms);

	EXPECT_TRUE(m_ended.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, DeadlineEndsAConversationThatNeverEnds)
{
	Answer(Start + 100ms);
	Tick(Start + 4999ms);

	EXPECT_TRUE(m_ended.empty());

	Tick(Start + 5s);

	EXPECT_EQ(m_ended.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, SixtyFifthDatagramIsOverTheCap)
{
	for (uint32_t index{ 0 }; index < 64; ++index)
	{
		EXPECT_FALSE(m_scheduler.OnDatagram(MakeRequest(1).address, 100, Start + 100ms)->isOverCap) << index;
	}

	EXPECT_TRUE(m_scheduler.OnDatagram(MakeRequest(1).address, 100, Start + 100ms)->isOverCap);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CConversationClockTest, BytesPastTheCapAreOverIt)
{
	EXPECT_FALSE(m_scheduler.OnDatagram(MakeRequest(1).address, 200u << 10, Start + 100ms)->isOverCap);
	EXPECT_TRUE(m_scheduler.OnDatagram(MakeRequest(1).address, 100u << 10, Start + 100ms)->isOverCap);
}
} // namespace
} // namespace Lkt::Net
