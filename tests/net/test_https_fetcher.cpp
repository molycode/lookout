#include "loopback_https_server.hpp"
#include "net/https_fetcher.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Lkt::Net
{
namespace
{
constexpr std::chrono::seconds Patience{ 10 };
constexpr uint32_t Loopback{ 0x7F000001 };
constexpr size_t MaxSize{ 1024 };

//////////////////////////////////////////////////////////////////////////
std::string BodyOrError(SFetchResult const& result)
{
	return result.body.has_value() ? *result.body : std::string{ "error: " } + result.body.error();
}

//////////////////////////////////////////////////////////////////////////
class CHttpsFetcherTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_server.Start());
	}

	void TearDown() override
	{
		m_fetcher.Terminate();
		m_server.Stop();
	}
	// ~testing::Test

	SHttpsOrigin MakeOrigin(std::string_view caFile = "ca.pem") const
	{
		SHttpsOrigin origin{};

		origin.host = "localhost";
		origin.port = m_server.GetPort();
		origin.userAgent = "Lookout tests";
		origin.timeout = std::chrono::seconds{ 2 };
		origin.caFile = (std::filesystem::path{ LKT_FIXTURES_DIR } / "tls" / caFile).string();
		origin.address = Query::SServerAddress{ Loopback, m_server.GetPort() };

		return origin;
	}

	bool StartFetcher(SHttpsOrigin origin)
	{
		return m_fetcher.Initialize(std::move(origin), [this]()
		{
			{
				std::lock_guard const lock{ m_mutex };

				m_hasResults = true;
			}

			m_wake.notify_one();
		});
	}

	// Waits until count results came, or gives up after Patience.
	std::vector<SFetchResult> WaitForResults(size_t count)
	{
		std::vector<SFetchResult> results{};
		auto const deadline{ std::chrono::steady_clock::now() + Patience };

		m_fetcher.TakeResults(results);

		while (results.size() < count && std::chrono::steady_clock::now() < deadline)
		{
			{
				std::unique_lock lock{ m_mutex };

				m_wake.wait_until(lock, deadline, [this]() { return m_hasResults; });
				m_hasResults = false;
			}

			m_fetcher.TakeResults(results);
		}

		return results;
	}

	std::vector<SFetchResult> Fetch(std::vector<SFetchRequest> requests)
	{
		size_t const count{ requests.size() };

		m_fetcher.Fetch(std::move(requests));

		return WaitForResults(count);
	}

	Fixtures::CLoopbackHttpsServer m_server;
	CHttpsFetcher m_fetcher;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_hasResults{ false };
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, FetchesAFile)
{
	m_server.SetReply("/index.json", Fixtures::SHttpsReply{ .body = "{ \"index\": 1 }" });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/index.json", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().path, "/index.json");
	EXPECT_EQ(BodyOrError(results.front()), "{ \"index\": 1 }");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, RequestsShareOneConnection)
{
	m_server.SetReply("/a", Fixtures::SHttpsReply{ .body = "a" });
	m_server.SetReply("/b", Fixtures::SHttpsReply{ .body = "b" });
	m_server.SetReply("/c", Fixtures::SHttpsReply{ .body = "c" });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/a", MaxSize }, { "/b", MaxSize }, { "/c", MaxSize } }) };

	ASSERT_EQ(results.size(), 3u);
	EXPECT_EQ(results[2].body.value_or(""), "c");
	EXPECT_EQ(m_server.GetNumConnections(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, MissingFileIsAnError)
{
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/absent", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().body.error_or(""), "localhost: answered 404 Not Found");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, RedirectIsNotFollowed)
{
	m_server.SetReply("/moved", Fixtures::SHttpsReply{ .status = "301 Moved Permanently", .headers = { "Location: https://example.org/" } });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/moved", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().body.error_or(""), "localhost: redirects to https://example.org/, which Lookout does not follow");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, ChunkedReplyIsRefused)
{
	m_server.SetReply("/chunked", Fixtures::SHttpsReply{ .body = "chunks", .headers = { "Transfer-Encoding: chunked" } });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/chunked", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().body.error_or(""), "localhost: sent the reply chunked-encoded, which Lookout does not read");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, ReplyLargerThanAllowedIsRefused)
{
	m_server.SetReply("/large", Fixtures::SHttpsReply{ .body = std::string(MaxSize + 1, 'x') });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/large", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().body.error_or(""), "localhost: sent 1025 bytes, more than the 1024 the file may have");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, UntrustedCertificateIsRefused)
{
	m_server.SetReply("/index.json", Fixtures::SHttpsReply{ .body = "{}" });
	ASSERT_TRUE(StartFetcher(MakeOrigin("other-ca.pem")));

	std::vector<SFetchResult> const results{ Fetch({ { "/index.json", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_TRUE(results.front().body.error_or("").starts_with("localhost: its certificate is not trusted: ")) << results.front().body.error_or("");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, CertificateForAnotherHostIsRefused)
{
	SHttpsOrigin origin{ MakeOrigin() };

	origin.host = "example.org";
	m_server.SetReply("/index.json", Fixtures::SHttpsReply{ .body = "{}" });
	ASSERT_TRUE(StartFetcher(std::move(origin)));

	std::vector<SFetchResult> const results{ Fetch({ { "/index.json", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_TRUE(results.front().body.error_or("").starts_with("example.org: its certificate is not trusted: ")) << results.front().body.error_or("");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, SilentServerTimesOut)
{
	m_server.SetReply("/slow", Fixtures::SHttpsReply{ .isSilent = true });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/slow", MaxSize } }) };

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().body.error_or(""), "localhost: did not answer within 2 s");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, RefusedConnectionFailsEveryRequest)
{
	SHttpsOrigin origin{ MakeOrigin() };
	uint16_t const port{ m_server.GetPort() };

	m_server.Stop();
	origin.address = Query::SServerAddress{ Loopback, port };
	ASSERT_TRUE(StartFetcher(std::move(origin)));

	std::vector<SFetchResult> const results{ Fetch({ { "/a", MaxSize }, { "/b", MaxSize } }) };

	ASSERT_EQ(results.size(), 2u);
	EXPECT_EQ(results[1].body.error_or(""), "localhost: cannot be reached: Connection refused");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, DroppedConnectionIsOpenedAgain)
{
	m_server.SetReply("/a", Fixtures::SHttpsReply{ .body = "a", .dropsConnection = true });
	m_server.SetReply("/b", Fixtures::SHttpsReply{ .body = "b" });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	std::vector<SFetchResult> const results{ Fetch({ { "/a", MaxSize }, { "/b", MaxSize } }) };

	ASSERT_EQ(results.size(), 2u);
	EXPECT_EQ(BodyOrError(results[1]), "b");
	EXPECT_EQ(m_server.GetNumConnections(), 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CHttpsFetcherTest, CancelledRequestsAreNotAnswered)
{
	m_server.SetReply("/slow", Fixtures::SHttpsReply{ .isSilent = true });
	ASSERT_TRUE(StartFetcher(MakeOrigin()));

	m_fetcher.Fetch({ { "/slow", MaxSize }, { "/slow", MaxSize } });
	m_fetcher.Cancel();
	std::this_thread::sleep_for(std::chrono::seconds{ 3 });

	std::vector<SFetchResult> results{};

	m_fetcher.TakeResults(results);
	EXPECT_TRUE(results.empty());
}
} // namespace
} // namespace Lkt::Net
