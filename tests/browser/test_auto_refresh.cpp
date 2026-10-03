#include "browser/auto_refresh.hpp"
#include <gtest/gtest.h>
#include <chrono>

namespace Lkt::Browser
{
namespace
{
constexpr std::chrono::seconds Interval{ 120 };
constexpr Net::Clock::time_point Start{ std::chrono::hours{ 1 } };

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, NeverRefreshedGameHasNoDeadline)
{
	CAutoRefresh const autoRefresh{};

	EXPECT_FALSE(autoRefresh.GetDeadline(Query::EGame::Quake3, Interval).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, DeadlineIsOneIntervalAfterTheStart)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.OnRefreshStarted(Query::EGame::Quake3, Start);

	EXPECT_EQ(autoRefresh.GetDeadline(Query::EGame::Quake3, Interval), Start + Interval);
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, ZeroIntervalHasNoDeadline)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.OnRefreshStarted(Query::EGame::Quake3, Start);

	EXPECT_FALSE(autoRefresh.GetDeadline(Query::EGame::Quake3, std::chrono::seconds{ 0 }).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, NewRefreshRestartsTheInterval)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.OnRefreshStarted(Query::EGame::Quake3, Start);
	autoRefresh.OnRefreshStarted(Query::EGame::Quake3, Start + std::chrono::seconds{ 100 });

	EXPECT_EQ(autoRefresh.GetDeadline(Query::EGame::Quake3, Interval), Start + std::chrono::seconds{ 100 } + Interval);
}

//////////////////////////////////////////////////////////////////////////
TEST(AutoRefresh, EachGameKeepsItsOwnTime)
{
	CAutoRefresh autoRefresh{};

	autoRefresh.OnRefreshStarted(Query::EGame::Kingpin, Start);

	EXPECT_FALSE(autoRefresh.GetDeadline(Query::EGame::Quake3, Interval).has_value());
}
} // namespace
} // namespace Lkt::Browser
