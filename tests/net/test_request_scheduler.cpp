#include "request_scheduler.hpp"
#include <gtest/gtest.h>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;

Clock::time_point const Start{ Clock::time_point{} + 1h };

//////////////////////////////////////////////////////////////////////////
SServerRequest MakeRequest(uint32_t index, Query::EGame game = Query::EGame::Kingpin)
{
	return SServerRequest{ game, Query::SServerAddress{ 0x2D000000 + index, 27960 } };
}

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
TEST(RequestScheduler, AnswerReportsTheRoundTrip)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);

	std::optional<SAnsweredRequest> const answered{ scheduler.Answer(MakeRequest(1).address, Start + 80ms) };

	ASSERT_TRUE(answered.has_value());
	EXPECT_EQ(answered->roundTrip, 80ms);
	EXPECT_FALSE(scheduler.HasWork(Query::EGame::Kingpin));
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, AnswerFromAnUnaskedServerIsIgnored)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);

	EXPECT_FALSE(scheduler.Answer(MakeRequest(2).address, Start + 80ms).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, RetriesOnceAfterATimeout)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};
	std::vector<SServerRequest> expired{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);
	due.clear();
	scheduler.TakeExpired(Start + 1s, expired);
	scheduler.TakeDue(Start + 1s, due);

	EXPECT_TRUE(expired.empty());
	ASSERT_EQ(due.size(), 1u);
	EXPECT_EQ(due[0].numAttempts, 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, GivesUpAfterTheSecondTimeout)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};
	std::vector<SServerRequest> expired{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);
	scheduler.TakeExpired(Start + 1s, expired);
	scheduler.TakeDue(Start + 1s, due);
	scheduler.TakeExpired(Start + 2s, expired);

	ASSERT_EQ(expired.size(), 1u);
	EXPECT_FALSE(scheduler.HasWork(Query::EGame::Kingpin));
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, QueuedIsPerGame)
{
	CRequestScheduler scheduler{};

	scheduler.Add(MakeRequest(1, Query::EGame::Quake2));

	EXPECT_FALSE(scheduler.IsQueued(Query::EGame::Kingpin, MakeRequest(1).address));
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
			scheduler.Answer(due[index].address, Start + millisecond * 1ms);
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
TEST(RequestScheduler, NotExpiredJustBeforeTheTimeout)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};
	std::vector<SServerRequest> expired{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);
	due.clear();
	scheduler.TakeExpired(Start + 999ms, expired);
	scheduler.TakeDue(Start + 999ms, due);

	EXPECT_TRUE(expired.empty());
	EXPECT_TRUE(due.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, CancelDropsRequestsInFlight)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
	scheduler.TakeDue(Start, due);
	scheduler.Cancel(Query::EGame::Kingpin);

	EXPECT_FALSE(scheduler.HasWork(Query::EGame::Kingpin));
	EXPECT_FALSE(scheduler.Answer(MakeRequest(1).address, Start + 10ms).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, CancelOnlyDropsThatGame)
{
	CRequestScheduler scheduler{};

	scheduler.Add(MakeRequest(1, Query::EGame::Kingpin));
	scheduler.Add(MakeRequest(2, Query::EGame::Quake2));
	scheduler.Cancel(Query::EGame::Kingpin);

	EXPECT_FALSE(scheduler.HasWork(Query::EGame::Kingpin));
	EXPECT_TRUE(scheduler.HasWork(Query::EGame::Quake2));
}

//////////////////////////////////////////////////////////////////////////
TEST(RequestScheduler, NextDeadlineIsTheTimeout)
{
	CRequestScheduler scheduler{};
	std::vector<SServerRequest> due{};

	scheduler.Add(MakeRequest(1));
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
	scheduler.Answer(MakeRequest(2).address, Start + 10ms);

	EXPECT_TRUE(scheduler.IsQueued(Query::EGame::Kingpin, MakeRequest(1).address));
	EXPECT_FALSE(scheduler.IsQueued(Query::EGame::Kingpin, MakeRequest(2).address));
}
} // namespace
} // namespace Lkt::Net
