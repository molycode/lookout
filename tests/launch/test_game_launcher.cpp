#include "channels.hpp"
#include "launch/game_launcher.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <pthread.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace Lkt::Launch
{
namespace
{
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

// 203.0.113.7:31510.
constexpr Query::SServerAddress Server{ 0xCB007107, 31510 };
constexpr std::string_view Address{ "203.0.113.7:31510" };
constexpr int LeakProbeDescriptor{ 100 };

constexpr std::filesystem::perms Executable{ std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read };

//////////////////////////////////////////////////////////////////////////
std::string ReadText(std::filesystem::path const& path)
{
	std::ifstream file{ path, std::ios::binary };

	return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::string> ReadLines(std::filesystem::path const& path)
{
	std::vector<std::string> lines{};
	std::ifstream file{ path };
	std::string line{};

	while (std::getline(file, line))
	{
		lines.emplace_back(line);
	}

	return lines;
}

//////////////////////////////////////////////////////////////////////////
// Blocks until the game exits but leaves it unreaped, for ReapFinished to collect.
pid_t WaitForExit()
{
	siginfo_t info{};

	EXPECT_EQ(::waitid(P_ALL, 0, &info, WEXITED | WNOWAIT), 0);

	return info.si_pid;
}

//////////////////////////////////////////////////////////////////////////
class CGameLauncherTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-game-XXXXXX").string() };

		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_root = pattern;
		m_logs = m_root / "logs";
		m_work = m_root / "work";
		m_script = m_root / "fake-game.sh";
		m_report = m_root / "report";
		std::filesystem::create_directories(m_logs);
		std::filesystem::create_directories(m_work);
		m_launcher.Initialize(m_logs.string());
	}

	void TearDown() override
	{
		std::error_code error{};

		// Children a test did not hand to ReapFinished.
		while (::waitpid(-1, nullptr, 0) > 0)
		{
		}

		std::filesystem::remove_all(m_root, error);
	}
	// ~testing::Test

	void WriteGame(std::string_view body) const
	{
		{
			std::ofstream file{ m_script, std::ios::binary };

			file << "#!/bin/sh\n" << body;
		}

		std::filesystem::permissions(m_script, Executable);
	}

	void WriteReportingGame(std::string_view ending) const
	{
		WriteGame(std::format(
			"exec >'{}'\n"
			"printf '%s\\n' \"$$\" \"$@\"\n"
			"pwd -P\n"
			"if [ /proc/$$/fd/0 -ef /dev/null ]; then echo stdin-null; else echo stdin-other; fi\n"
			"if [ -L /proc/$$/fd/{} ]; then echo probe-leaked; else echo probe-closed; fi\n"
			"{}\n", m_report.string(), LeakProbeDescriptor, ending));
	}

	SLaunchOption MakeOption() const
	{
		return SLaunchOption{ "test", "Fake game", m_work.string(), { m_script.string(), "--fake" }, m_work.string() };
	}

	std::expected<void, ELaunchError> Launch(std::string password = {})
	{
		return m_launcher.Launch(Query::GetGame(Query::EGame::Kingpin), MakeOption(), SConnectRequest{ Server, std::move(password) });
	}

	std::filesystem::path m_root;
	std::filesystem::path m_logs;
	std::filesystem::path m_work;
	std::filesystem::path m_script;
	std::filesystem::path m_report;
	CGameLauncher m_launcher;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, ArgumentsAreTheOptionsFollowedByTheConnectArguments)
{
	WriteReportingGame("exit 0");
	ASSERT_TRUE(Launch("s3cret").has_value());
	WaitForExit();

	std::vector<std::string> const lines{ ReadLines(m_report) };
	std::vector<std::string> const expected{ "--fake", "+set", "password", "s3cret", "+connect", std::string{ Address } };

	ASSERT_GE(lines.size(), expected.size() + 1);
	EXPECT_EQ(std::vector<std::string>(lines.begin() + 1, lines.begin() + 1 + static_cast<std::ptrdiff_t>(expected.size())), expected);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, GameRunsInTheOptionsWorkingDirectory)
{
	WriteReportingGame("exit 0");
	ASSERT_TRUE(Launch().has_value());
	WaitForExit();

	std::vector<std::string> const lines{ ReadLines(m_report) };

	ASSERT_EQ(lines.size(), 7u);
	EXPECT_EQ(lines[4], std::filesystem::canonical(m_work).string());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, GameGetsASessionOfItsOwn)
{
	WriteReportingGame("exit 0");
	ASSERT_TRUE(Launch().has_value());

	pid_t const pid{ WaitForExit() };

	EXPECT_EQ(::getsid(pid), pid);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, StdinIsDevNull)
{
	WriteReportingGame("exit 0");
	ASSERT_TRUE(Launch().has_value());
	WaitForExit();

	std::vector<std::string> const lines{ ReadLines(m_report) };

	ASSERT_EQ(lines.size(), 7u);
	EXPECT_EQ(lines[5], "stdin-null");
}

//////////////////////////////////////////////////////////////////////////
// The probe is inheritable on purpose: only the spawn's own closing can keep it from the game.
TEST_F(CGameLauncherTest, OpenDescriptorDoesNotReachTheGame)
{
	int const devNull{ ::open("/dev/null", O_RDONLY) };
	int const probe{ ::fcntl(devNull, F_DUPFD, LeakProbeDescriptor) };

	::close(devNull);
	ASSERT_EQ(probe, LeakProbeDescriptor);
	WriteReportingGame("exit 0");
	ASSERT_TRUE(Launch().has_value());
	WaitForExit();
	::close(probe);

	std::vector<std::string> const lines{ ReadLines(m_report) };

	ASSERT_EQ(lines.size(), 7u);
	EXPECT_EQ(lines[6], "probe-closed");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, OutputLandsInTheGameLog)
{
	WriteGame("echo out\necho err >&2\n");
	ASSERT_TRUE(Launch().has_value());
	WaitForExit();

	EXPECT_EQ(ReadText(m_logs / "game-kingpin.log"), "out\nerr\n");
}

//////////////////////////////////////////////////////////////////////////
// With stdin closed, the log opens as descriptor 0, so opening the game's stdin first would discard its output.
TEST_F(CGameLauncherTest, OutputLandsInTheGameLogWhenStdinIsClosed)
{
	int const savedStdin{ ::dup(STDIN_FILENO) };

	WriteGame("echo out\n");
	::close(STDIN_FILENO);

	std::expected<void, ELaunchError> const launched{ Launch() };

	::dup2(savedStdin, STDIN_FILENO);
	::close(savedStdin);
	ASSERT_TRUE(launched.has_value());
	WaitForExit();

	EXPECT_EQ(ReadText(m_logs / "game-kingpin.log"), "out\n");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, UnwritableGameLogStillStartsTheGame)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };
	std::filesystem::path const notADirectory{ m_root / "not-a-directory" };

	std::ofstream{ notADirectory };
	m_launcher.Initialize(notADirectory.string());
	WriteGame("exit 0\n");

	EXPECT_TRUE(Launch().has_value());
	WaitForExit();
	m_launcher.ReapFinished();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, NonZeroExitIsAWarning)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteGame("exit 3\n");
	ASSERT_TRUE(Launch().has_value());
	WaitForExit();
	m_launcher.ReapFinished();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, CleanExitIsSilent)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteGame("exit 0\n");
	ASSERT_TRUE(Launch().has_value());
	WaitForExit();
	m_launcher.ReapFinished();
}

//////////////////////////////////////////////////////////////////////////
// A shell keeps a signal that was ignored when it started, so the game dying of SIGPIPE proves it started at default.
TEST_F(CGameLauncherTest, IgnoredSignalStartsAtItsDefault)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };
	struct sigaction ignore{};
	struct sigaction saved{};

	ignore.sa_handler = SIG_IGN;
	::sigaction(SIGPIPE, &ignore, &saved);
	WriteGame("kill -s PIPE $$\nexit 0\n");

	std::expected<void, ELaunchError> const launched{ Launch() };

	::sigaction(SIGPIPE, &saved, nullptr);
	ASSERT_TRUE(launched.has_value());
	WaitForExit();
	m_launcher.ReapFinished();
}

//////////////////////////////////////////////////////////////////////////
// Left blocked, the signal would wait until the game exited cleanly.
TEST_F(CGameLauncherTest, BlockedSignalStartsUnblocked)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };
	sigset_t blocked{};
	sigset_t saved{};

	sigemptyset(&blocked);
	sigaddset(&blocked, SIGUSR2);
	::pthread_sigmask(SIG_BLOCK, &blocked, &saved);
	WriteGame("kill -s USR2 $$\nexit 0\n");

	std::expected<void, ELaunchError> const launched{ Launch() };

	::pthread_sigmask(SIG_SETMASK, &saved, nullptr);
	ASSERT_TRUE(launched.has_value());
	WaitForExit();
	m_launcher.ReapFinished();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, GameReapedElsewhereIsForgottenAfterOneWarning)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteGame("exit 0\n");
	ASSERT_TRUE(Launch().has_value());
	::waitpid(WaitForExit(), nullptr, 0);
	m_launcher.ReapFinished();
	m_launcher.ReapFinished();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, MissingProgramIsAnError)
{
	CExpectedLog const expected{ LaunchChannel, 0, 1 };
	SLaunchOption const option{ "test", "Fake game", {}, { (m_root / "no-such-game").string() }, {} };

	EXPECT_EQ(m_launcher.Launch(Query::GetGame(Query::EGame::Kingpin), option, SConnectRequest{ Server, {} }).error_or(ELaunchError::NoLauncher),
		ELaunchError::SpawnFailed);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGameLauncherTest, UnsupportedPasswordStartsNothing)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };
	siginfo_t info{};

	WriteGame("exit 0\n");

	EXPECT_EQ(Launch("open sesame").error_or(ELaunchError::NoLauncher), ELaunchError::UnsupportedPassword);

	int const waited{ ::waitid(P_ALL, 0, &info, WEXITED | WNOHANG | WNOWAIT) };
	int const waitError{ errno };

	EXPECT_EQ(waited, -1);
	EXPECT_EQ(waitError, ECHILD);
}
} // namespace
} // namespace Lkt::Launch
