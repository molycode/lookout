#include "net/loopback_https_server.hpp"
#include "download/latest_release.hpp"
#include "download/lookout_release.hpp"
#include "net/clock.hpp"
#include "net/https_origin.hpp"
#include "query/server_address.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <format>
#include <mutex>
#include <string_view>
#include <utility>

namespace Lkt::Download
{
namespace
{
constexpr std::chrono::seconds Patience{ 10 };
constexpr uint32_t Loopback{ 0x7F000001 };

//////////////////////////////////////////////////////////////////////////
// GitHub's releases API on loopback; time is passed in, so a day can go by in a test.
class CLatestReleaseTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_server.Start());

		Net::SHttpsOrigin origin{};

		origin.host = "localhost";
		origin.port = m_server.GetPort();
		origin.userAgent = "Lookout tests";
		origin.caFile = (std::filesystem::path{ LKT_FIXTURES_DIR } / "tls" / "ca.pem").string();
		origin.address = Query::SServerAddress{ Loopback, m_server.GetPort() };
		ASSERT_TRUE(m_release.Initialize(std::move(origin), [this]()
		{
			{
				std::lock_guard const lock{ m_mutex };

				m_hasResults = true;
			}

			m_wake.notify_one();
		}));
	}

	void TearDown() override
	{
		m_release.Terminate();
		m_server.Stop();
	}
	// ~testing::Test

	void ServeTag(std::string_view tag)
	{
		m_server.SetReply(LookoutLatestReleasePath, Fixtures::SHttpsReply{ .body = std::format(R"({{ "tag_name": "{}" }})", tag) });
	}

	// One Update at now, then more until the check it may have started is answered.
	void UpdateAt(Net::Clock::time_point now)
	{
		auto const deadline{ std::chrono::steady_clock::now() + Patience };

		m_release.Update(now);

		while (m_release.IsChecking() && std::chrono::steady_clock::now() < deadline)
		{
			{
				std::unique_lock lock{ m_mutex };

				m_wake.wait_until(lock, deadline, [this]() { return m_hasResults; });
				m_hasResults = false;
			}

			m_release.Update(now);
		}
	}

	Fixtures::CLoopbackHttpsServer m_server;
	CLatestRelease m_release;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_hasResults{ false };
	Net::Clock::time_point const m_start{ Net::Clock::now() };
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CLatestReleaseTest, FirstUpdateReadsTheVersion)
{
	ServeTag("v9.0.41");
	UpdateAt(m_start);

	EXPECT_EQ(m_release.GetVersion(), "9.0.41");
	EXPECT_TRUE(m_release.GetProblem().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLatestReleaseTest, AnswerIsCheckedAgainADayLater)
{
	ServeTag("v9.0.41");
	UpdateAt(m_start);

	EXPECT_EQ(m_release.GetNextCheck(), m_start + std::chrono::hours{ 24 });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLatestReleaseTest, NothingIsAskedBeforeTheCheckIsDue)
{
	ServeTag("v9.0.41");
	UpdateAt(m_start);
	m_release.Update(m_start + std::chrono::hours{ 23 });

	EXPECT_FALSE(m_release.IsChecking());
	EXPECT_EQ(m_server.GetNumRequests(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLatestReleaseTest, FailureIsTriedAgainWithinTheHour)
{
	UpdateAt(m_start);

	EXPECT_TRUE(m_release.GetVersion().empty());
	EXPECT_FALSE(m_release.GetProblem().empty());
	EXPECT_EQ(m_release.GetNextCheck(), m_start + std::chrono::hours{ 1 });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLatestReleaseTest, AnswerAfterAFailureClearsTheProblem)
{
	UpdateAt(m_start);
	ServeTag("v9.0.41");
	UpdateAt(m_start + std::chrono::hours{ 1 });

	EXPECT_EQ(m_release.GetVersion(), "9.0.41");
	EXPECT_TRUE(m_release.GetProblem().empty());
}

//////////////////////////////////////////////////////////////////////////
// GitHub's rate limit answers 403; the version from the day before is still the best known.
TEST_F(CLatestReleaseTest, FailureKeepsTheVersionFoundBefore)
{
	ServeTag("v9.0.41");
	UpdateAt(m_start);
	m_server.SetReply(LookoutLatestReleasePath, Fixtures::SHttpsReply{ .status = "403 Forbidden" });
	UpdateAt(m_start + std::chrono::hours{ 24 });

	EXPECT_EQ(m_release.GetVersion(), "9.0.41");
	EXPECT_FALSE(m_release.GetProblem().empty());
}
} // namespace
} // namespace Lkt::Download
