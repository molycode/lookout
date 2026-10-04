#include "fixtures.hpp"
#include "net/event_collector.hpp"
#include "net/loopback_server.hpp"
#include "net/query_engine.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <variant>

namespace Lkt::Net
{
namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds Patience{ 5s };

//////////////////////////////////////////////////////////////////////////
template<typename TEvent>
size_t CountEvents(std::span<SQueryEvent const> events)
{
	return static_cast<size_t>(std::ranges::count_if(events, [](SQueryEvent const& event) { return std::holds_alternative<TEvent>(event); }));
}

//////////////////////////////////////////////////////////////////////////
uint32_t GetRefreshId(SQueryEvent const& event)
{
	return std::visit([](auto const& typed) { return typed.refreshId; }, event);
}

//////////////////////////////////////////////////////////////////////////
bool AllCarry(std::span<SQueryEvent const> events, uint32_t refreshId)
{
	return std::ranges::all_of(events, [refreshId](SQueryEvent const& event) { return GetRefreshId(event) == refreshId; });
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryEngine, AnswersAServerItWasAskedAbout)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer server{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(server.Start(Fixtures::LoadFixture("kingpin/status-93.226.82.165_31510.bin")));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, server.GetAddress());

	bool const isFinished{ collector.WaitForFinish(engine, game, Patience) };

	engine.Terminate();
	server.Stop();

	ASSERT_TRUE(isFinished);

	auto const answered{ std::ranges::find_if(collector.GetEvents(), [](SQueryEvent const& event) { return std::holds_alternative<SServerAnswered>(event); }) };

	ASSERT_NE(answered, collector.GetEvents().end());
	EXPECT_EQ(Query::FindRule(std::get<SServerAnswered>(*answered).reply, "hostname"), "BM - kp.satoki.org");
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryEngine, SilentServerIsAskedTwiceThenFails)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer server{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(server.Start({}));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, server.GetAddress());

	bool const isFinished{ collector.WaitForFinish(engine, game, Patience) };

	engine.Terminate();
	server.Stop();

	ASSERT_TRUE(isFinished);
	EXPECT_EQ(CountEvents<SServerFailed>(collector.GetEvents()), 1u);
	EXPECT_EQ(server.GetNumRequests(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryEngine, RetrySendsWhatTheConversationStartedWith)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer server{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(server.Start({}));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, server.GetAddress());

	bool const isFinished{ collector.WaitForFinish(engine, game, Patience) };

	engine.Terminate();
	server.Stop();

	ASSERT_TRUE(isFinished);
	EXPECT_EQ(server.GetFirstRequest(), Fixtures::ToBytes("\xFF\xFF\xFF\xFFstatus\n"));
	EXPECT_TRUE(server.AreRequestsAlike());
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryEngine, RunsAgainAfterARefreshFinished)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer server{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(server.Start(Fixtures::LoadFixture("kingpin/status-93.226.82.165_31510.bin")));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, server.GetAddress());

	bool const isFirstFinished{ collector.WaitForFinish(engine, game, Patience) };

	engine.RefreshServer(game, server.GetAddress());

	bool const isSecondFinished{ collector.WaitForFinish(engine, game, Patience) };

	engine.Terminate();
	server.Stop();

	EXPECT_TRUE(isFirstFinished);
	EXPECT_TRUE(isSecondFinished);
	EXPECT_EQ(CountEvents<SServerAnswered>(collector.GetEvents()), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryEngine, TerminateDuringARefreshReturnsPromptly)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer server{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(server.Start({}));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, server.GetAddress());

	auto const started{ std::chrono::steady_clock::now() };

	engine.Terminate();

	auto const elapsed{ std::chrono::steady_clock::now() - started };

	server.Stop();

	EXPECT_LT(elapsed, 500ms);
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryEngine, EachRefreshCarriesALargerIdThanTheOneBefore)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer server{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(server.Start(Fixtures::LoadFixture("kingpin/status-93.226.82.165_31510.bin")));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, server.GetAddress());

	bool const isFirstFinished{ collector.WaitForFinish(engine, game, Patience) };
	size_t const numFirstEvents{ collector.GetEvents().size() };

	engine.RefreshServer(game, server.GetAddress());

	bool const isSecondFinished{ collector.WaitForFinish(engine, game, Patience) };

	engine.Terminate();
	server.Stop();

	ASSERT_TRUE(isFirstFinished && isSecondFinished);

	std::span<SQueryEvent const> const events{ collector.GetEvents() };
	uint32_t const firstId{ GetRefreshId(events.front()) };
	uint32_t const secondId{ GetRefreshId(events.back()) };

	EXPECT_GT(firstId, 0u);
	EXPECT_GT(secondId, firstId);
	EXPECT_TRUE(AllCarry(events.first(numFirstEvents), firstId));
	EXPECT_TRUE(AllCarry(events.subspan(numFirstEvents), secondId));
}

//////////////////////////////////////////////////////////////////////////
// The silent server keeps the first refresh running, so the second request joins it.
TEST(QueryEngine, ServerAskedDuringARefreshCarriesThatRefreshsId)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer silent{};
	Fixtures::CLoopbackServer answering{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(silent.Start({}));
	ASSERT_TRUE(answering.Start(Fixtures::LoadFixture("kingpin/status-93.226.82.165_31510.bin")));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, silent.GetAddress());
	engine.RefreshServer(game, answering.GetAddress());

	bool const isFinished{ collector.WaitForFinish(engine, game, Patience) };

	engine.Terminate();
	silent.Stop();
	answering.Stop();

	ASSERT_TRUE(isFinished);
	EXPECT_EQ(CountEvents<SServerAnswered>(collector.GetEvents()), 1u);
	EXPECT_TRUE(AllCarry(collector.GetEvents(), GetRefreshId(collector.GetEvents().front())));
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryEngine, RefreshAfterACancelCarriesTheNewId)
{
	Query::EGame const game{ Fixtures::GetGameId("kingpin") };
	Fixtures::CLoopbackServer silent{};
	Fixtures::CLoopbackServer answering{};
	Fixtures::CEventCollector collector{};
	CQueryEngine engine{};

	ASSERT_TRUE(silent.Start({}));
	ASSERT_TRUE(answering.Start(Fixtures::LoadFixture("kingpin/status-93.226.82.165_31510.bin")));
	ASSERT_TRUE(engine.Initialize(collector.MakeCallback()));

	engine.RefreshServer(game, silent.GetAddress());
	engine.Cancel(game);
	engine.RefreshServer(game, answering.GetAddress());

	bool isWaiting{ true };
	bool hasAnswer{ false };

	while (isWaiting && !hasAnswer)
	{
		isWaiting = collector.WaitForFinish(engine, game, Patience);
		hasAnswer = CountEvents<SServerAnswered>(collector.GetEvents()) > 0;
	}

	engine.Terminate();
	silent.Stop();
	answering.Stop();

	ASSERT_TRUE(hasAnswer);

	std::span<SQueryEvent const> const events{ collector.GetEvents() };
	auto const cancelled{ std::ranges::find_if(events, [](SQueryEvent const& event) { return std::holds_alternative<SRefreshFinished>(event); }) };

	ASSERT_NE(cancelled, events.end());

	uint32_t const cancelledId{ GetRefreshId(*cancelled) };
	size_t const numUpToCancel{ static_cast<size_t>(cancelled - events.begin()) + 1 };

	EXPECT_TRUE(AllCarry(events.first(numUpToCancel), cancelledId));
	EXPECT_TRUE(std::ranges::all_of(events.subspan(numUpToCancel), [cancelledId](SQueryEvent const& event) { return GetRefreshId(event) > cancelledId; }));
}
} // namespace
} // namespace Lkt::Net
