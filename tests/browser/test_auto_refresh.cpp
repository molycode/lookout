#include "browser/auto_refresh.hpp"
#include <gtest/gtest.h>
#include <chrono>

namespace Lkt::Browser
{
namespace
{
constexpr std::chrono::seconds Interval{ 120 };
constexpr Net::Clock::time_point Start{ std::chrono::hours{ 1 } };
constexpr Query::EGame Game{ 0 };
constexpr Query::EGame OtherGame{ 1 };

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, NeverRefreshedGameHasNoDeadline)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.Initialize(2);

	EXPECT_FALSE(autoRefresh.GetDeadline(Game, Interval).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, DeadlineIsOneIntervalAfterTheStart)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.Initialize(2);
	autoRefresh.OnRefreshStarted(Game, Start);

	EXPECT_EQ(autoRefresh.GetDeadline(Game, Interval), Start + Interval);
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, ZeroIntervalHasNoDeadline)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.Initialize(2);
	autoRefresh.OnRefreshStarted(Game, Start);

	EXPECT_FALSE(autoRefresh.GetDeadline(Game, std::chrono::seconds{ 0 }).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, NewRefreshRestartsTheInterval)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.Initialize(2);
	autoRefresh.OnRefreshStarted(Game, Start);
	autoRefresh.OnRefreshStarted(Game, Start + std::chrono::seconds{ 100 });

	EXPECT_EQ(autoRefresh.GetDeadline(Game, Interval), Start + std::chrono::seconds{ 100 } + Interval);
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, EachGameKeepsItsOwnTime)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.Initialize(2);
	autoRefresh.OnRefreshStarted(OtherGame, Start);

	EXPECT_FALSE(autoRefresh.GetDeadline(Game, Interval).has_value());
}
} // namespace
} // namespace Lkt::Browser
