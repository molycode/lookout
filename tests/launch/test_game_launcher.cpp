#include "channels.hpp"
#include "fixtures.hpp"
#include "launch/game_launcher.hpp"
#include "query/game_definition.hpp"
#include "temp_dir.hpp"
#include <tge/platform.hpp>
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(TGE_PLATFORM_LINUX)
#include <charconv>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#endif // defined(TGE_PLATFORM_LINUX)

namespace Lkt::Launch
{
namespace
{
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

// 203.0.113.7:31510.
constexpr Query::SServerAddress Server{ 0xCB007107, 31510 };
constexpr std::string_view Address{ "203.0.113.7:31510" };
constexpr std::chrono::seconds Patience{ 10 };
constexpr std::chrono::milliseconds PollInterval{ 10 };

//////////////////////////////////////////////////////////////////////////
std::string ReadText(std::filesystem::path const& path)
{
	std::ifstream file{ path, std::ios::binary };

	return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

//////////////////////////////////////////////////////////////////////////
class CGameLauncherTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-game"));
		m_root = m_tempDir.GetPath();
		m_logs = m_root / "logs";
		m_work = m_root / "work";
		m_report = m_root / "report";
		std::filesystem::create_directories(m_logs);
		std::filesystem::create_directories(m_work);
		m_launcher.Initialize(m_logs.string());
	}

	// Windows cannot remove a folder a running game works in.
	void TearDown() override
	{
		WaitUntilAllEnded();
		m_launcher.Terminate();
		m_tempDir.Terminate();
	}
	// ~testing::Test

	// Reaps each game that ended, which logs what the launcher has to say about it.
	void WaitUntilAllEnded()
	{
		auto const deadline{ std::chrono::steady_clock::now() + Patience };
		size_t numRunning{ m_launcher.ReapFinished() };

		while (numRunning > 0 && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(PollInterval);
			numRunning = m_launcher.ReapFinished();
		}

		EXPECT_EQ(numRunning, 0u);
	}

	SLaunchOption MakeOption(std::vector<std::string> const& options) const
	{
		std::vector<std::string> argv{ LKT_FAKE_GAME };

		argv.insert(argv.end(), options.begin(), options.end());

		return SLaunchOption{ "test", "Fake game", m_work.string(), std::move(argv), m_work.string() };
	}

	std::expected<void, ELaunchError> Launch(std::vector<std::string> const& options, std::string password = {})
	{
		return m_launcher.Launch(Fixtures::GetGameByKey("kingpin"), MakeOption(options), SConnectRequest{ Server, std::move(password) });
	}

	std::string ReadGameLog() const
	{
		return ReadText(m_logs / "game-kingpin.log");
	}

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_root{};
	std::filesystem::path m_logs{};
	std::filesystem::path m_work{};
	std::filesystem::path m_report{};
	CGameLauncher m_launcher{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, ArgumentsAreTheOptionsFollowedByTheConnectArguments)
{
	ASSERT_TRUE(Launch({ "--fake" }, "s3cret").has_value());
	WaitUntilAllEnded();

	EXPECT_EQ(ReadGameLog(), std::format("--fake\n+set\npassword\ns3cret\n+connect\n{}\n", Address));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, GameRunsInTheOptionsWorkingDirectory)
{
	std::error_code error{};

	ASSERT_TRUE(Launch({ std::format("--cwd-to={}", m_report.string()) }).has_value());
	WaitUntilAllEnded();

	EXPECT_TRUE(std::filesystem::equivalent(std::filesystem::path{ ReadText(m_report) }, m_work, error)) << ReadText(m_report);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, OutputAndErrorsLandInTheGameLog)
{
	ASSERT_TRUE(Launch({ "--stderr=err", "out" }).has_value());
	WaitUntilAllEnded();

	EXPECT_EQ(ReadGameLog(), std::format("out\n+connect\n{}\nerr\n", Address));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, UnwritableGameLogStillStartsTheGame)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };
	std::filesystem::path const notADirectory{ m_root / "not-a-directory" };

	std::ofstream{ notADirectory };
	m_launcher.Initialize(notADirectory.string());

	EXPECT_TRUE(Launch({}).has_value());
	WaitUntilAllEnded();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, NonZeroExitIsAWarning)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	ASSERT_TRUE(Launch({ "--exit=3" }).has_value());
	WaitUntilAllEnded();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, CleanExitIsSilent)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	ASSERT_TRUE(Launch({}).has_value());
	WaitUntilAllEnded();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, MissingProgramIsAnError)
{
	CExpectedLog const expected{ LaunchChannel, 0, 1 };
	SLaunchOption const option{ "test", "Fake game", {}, { (m_root / "no-such-game").string() }, {} };

	EXPECT_EQ(m_launcher.Launch(Fixtures::GetGameByKey("kingpin"), option, SConnectRequest{ Server, {} }).error_or(ELaunchError::NoLauncher),
		ELaunchError::SpawnFailed);
}

//////////////////////////////////////////////////////////////////////////
// Refused before anything is started, so not even the game's log is opened.
TEST_F(CGameLauncherTest, UnsupportedPasswordStartsNothing)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };
	std::error_code error{};

	EXPECT_EQ(Launch({}, "open sesame").error_or(ELaunchError::NoLauncher), ELaunchError::UnsupportedPassword);
	EXPECT_FALSE(std::filesystem::exists(m_logs / "game-kingpin.log", error));
}

#if defined(TGE_PLATFORM_LINUX)
//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, GameEndedByASignalIsAWarning)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	ASSERT_TRUE(Launch({ std::format("--signal={}", SIGTERM) }).has_value());
	WaitUntilAllEnded();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, GameReapedElsewhereIsForgottenAfterOneWarning)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };
	auto const deadline{ std::chrono::steady_clock::now() + Patience };
	pid_t pid{ 0 };

	ASSERT_TRUE(Launch({ std::format("--pid-to={}", m_report.string()) }).has_value());

	while (pid == 0 && std::chrono::steady_clock::now() < deadline)
	{
		std::string const text{ ReadText(m_report) };

		std::from_chars(text.data(), text.data() + text.size(), pid);

		if (pid == 0)
		{
			std::this_thread::sleep_for(PollInterval);
		}
	}

	ASSERT_GT(pid, 0);
	ASSERT_EQ(::waitpid(pid, nullptr, 0), pid);
	WaitUntilAllEnded();
	m_launcher.ReapFinished();
}
#endif // defined(TGE_PLATFORM_LINUX)
} // namespace
} // namespace Lkt::Launch
