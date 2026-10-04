#include "browser/browser.hpp"
#include "browser/browser_waiter.hpp"
#include "channels.hpp"
#include "fixtures.hpp"
#include "net/loopback_server.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>

namespace Lkt::Browser
{
namespace
{
using namespace std::chrono_literals;
using Fixtures::BrowserChannel;
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

constexpr std::chrono::milliseconds Patience{ 5s };

constexpr std::filesystem::perms Executable{ std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read };

//////////////////////////////////////////////////////////////////////////
std::string ReadText(std::filesystem::path const& path)
{
	std::ifstream file{ path, std::ios::binary };

	return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

//////////////////////////////////////////////////////////////////////////
void WriteText(std::filesystem::path const& path, std::string_view text)
{
	std::ofstream file{ path, std::ios::binary };

	file << text;
}

//////////////////////////////////////////////////////////////////////////
// Never refreshes or selects a game: both would ask the real masters.
class CBrowserTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-browser-XXXXXX").string() };

		ASSERT_EQ(error.value(), 0);
		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_root = pattern;
		m_configDir = m_root / "config";
		m_logsDir = m_root / "logs";
		m_game = m_root / "fake-game.sh";
		m_environment.dataHome = m_root / "data";
		m_environment.home = m_root / "home";
		m_environment.searchPath = (m_root / "bin").string();
		std::filesystem::create_directories(m_configDir);
		std::filesystem::create_directories(m_logsDir);
		std::filesystem::create_directories(m_environment.home);
		WriteText(m_game, "#!/bin/sh\nprintf '%s\\n' \"$@\"\n");
		std::filesystem::permissions(m_game, Executable);

		// Quoted, so a TMPDIR with a space or a shell character still splits as one program.
		WriteConfig(std::format(R"({{ "game": "kingpin", "games": {{ "kingpin": {{ "installs": [{{ "id": 1, "command": "\"{}\"" }}] }} }} }})", m_game.string()));
	}

	void TearDown() override
	{
		std::error_code error{};

		m_browser.Terminate();
		m_server.Stop();

		// Children a test did not hand to the browser's reaping.
		while (::waitpid(-1, nullptr, 0) > 0)
		{
		}

		std::filesystem::remove_all(m_root, error);
	}
	// ~testing::Test

	void WriteConfig(std::string_view text) const
	{
		WriteText(m_configDir / "config.json", text);
	}

	void WriteDesktopEntry(std::string_view id, std::string_view text) const
	{
		std::filesystem::path const directory{ m_environment.dataHome / "applications" };

		std::filesystem::create_directories(directory);
		WriteText(directory / id, text);
	}

	void Initialize()
	{
		m_browser.Initialize(m_configDir.string(), m_logsDir.string(), m_environment);
	}

	bool StartWithServer()
	{
		Initialize();

		return m_server.Start(Fixtures::LoadFixture("kingpin/status-93.226.82.165_31510.bin")) && m_browser.Start(m_waiter.MakeCallback());
	}

	SServerEntry const* FindRow(Query::SServerAddress const& address) const
	{
		SServerEntry const* pFound{ nullptr };

		for (uint32_t const row : m_browser.GetRows())
		{
			SServerEntry const& entry{ m_browser.GetEntries()[row] };

			pFound = (entry.address == address) ? &entry : pFound;
		}

		return pFound;
	}

	std::filesystem::path m_root;
	std::filesystem::path m_configDir;
	std::filesystem::path m_logsDir;
	std::filesystem::path m_game;
	Launch::SLaunchEnvironment m_environment;
	Fixtures::CLoopbackServer m_server;
	Fixtures::CBrowserWaiter m_waiter;
	CBrowser m_browser;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, AddedServerShowsAtOnceAsAWaitingFavourite)
{
	ASSERT_TRUE(StartWithServer());
	ASSERT_TRUE(m_browser.AddServer(Query::FormatAddress(m_server.GetAddress())).has_value());

	SServerEntry const* const pRow{ FindRow(m_server.GetAddress()) };

	ASSERT_NE(pRow, nullptr);
	EXPECT_TRUE(pRow->isFavourite);
	EXPECT_EQ(pRow->state, EServerState::Pending);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, AddedServerIsSavedAtOnce)
{
	ASSERT_TRUE(StartWithServer());
	ASSERT_TRUE(m_browser.AddServer(Query::FormatAddress(m_server.GetAddress())).has_value());

	EXPECT_TRUE(ReadText(m_configDir / "config.json").contains(Query::FormatAddress(m_server.GetAddress())));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, AddedServerComesOnline)
{
	ASSERT_TRUE(StartWithServer());
	ASSERT_TRUE(m_browser.AddServer(Query::FormatAddress(m_server.GetAddress())).has_value());

	EXPECT_TRUE(m_waiter.WaitUntil(m_browser, [this]()
	{
		SServerEntry const* const pRow{ FindRow(m_server.GetAddress()) };

		return pRow != nullptr && pRow->state == EServerState::Online;
	}, Patience));
	EXPECT_EQ(m_browser.GetStatus(Fixtures::GetGameId("kingpin")).numAnswered, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, InvalidAddressIsAWarning)
{
	CExpectedLog const expected{ BrowserChannel, 1, 0 };

	Initialize();

	EXPECT_EQ(m_browser.AddServer("kp.example.org:31510").error_or(Query::EParseError::Truncated), Query::EParseError::Malformed);
	m_browser.Terminate();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, UnmarkedFavouriteIsSavedAtOnce)
{
	ASSERT_TRUE(StartWithServer());
	ASSERT_TRUE(m_browser.AddServer(Query::FormatAddress(m_server.GetAddress())).has_value());

	m_browser.ToggleFavourite(m_server.GetAddress());

	EXPECT_FALSE(m_browser.IsFavourite(m_server.GetAddress()));
	EXPECT_FALSE(ReadText(m_configDir / "config.json").contains(Query::FormatAddress(m_server.GetAddress())));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, JoinStartsTheGameWithTheAddress)
{
	Query::SServerAddress const address{ 0xCB007107, 31510 };
	siginfo_t info{};

	// Started, so the spawn happens beside the engine's thread, as in the app.
	ASSERT_TRUE(StartWithServer());
	ASSERT_TRUE(m_browser.Join(address, {}, "install:1").has_value());
	ASSERT_EQ(::waitid(P_ALL, 0, &info, WEXITED | WNOWAIT), 0);
	m_browser.Update();

	EXPECT_EQ(ReadText(m_logsDir / "game-kingpin.log"), "+connect\n203.0.113.7:31510\n");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, JoinWithoutALauncherIsAWarning)
{
	CExpectedLog const expected{ BrowserChannel, 1, 0 };

	WriteConfig(R"({ "game": "kingpin" })");
	Initialize();

	EXPECT_EQ(m_browser.Join(Query::SServerAddress{ 0xCB007107, 31510 }, {}, "").error_or(Launch::ELaunchError::SpawnFailed), Launch::ELaunchError::NoLauncher);
	m_browser.Terminate();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, JoinWithAMissingLauncherIsAWarning)
{
	CExpectedLog const expected{ BrowserChannel, 1, 0 };

	Initialize();

	EXPECT_EQ(m_browser.Join(Query::SServerAddress{ 0xCB007107, 31510 }, {}, "kingpin-native.desktop").error_or(Launch::ELaunchError::SpawnFailed),
		Launch::ELaunchError::ChosenLauncherMissing);
	m_browser.Terminate();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, LaunchOptionsHoldTheGamesDesktopEntry)
{
	WriteDesktopEntry("kingpin-native.desktop", std::format("[Desktop Entry]\nType=Application\nName=Kingpin\nExec=\"{}\"\n", m_game.string()));
	Initialize();

	std::span<Launch::SLaunchOption const> const options{ m_browser.GetLaunchOptions(Fixtures::GetGameId("kingpin")) };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options.front().id, "kingpin-native.desktop");
	EXPECT_EQ(options.front().argv, std::vector<std::string>{ m_game.string() });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, UnselectedGameHasItsLaunchOptions)
{
	WriteDesktopEntry("id-linux-rtcw-mp.desktop", std::format("[Desktop Entry]\nType=Application\nName=RTCW\nExec=\"{}\"\n", m_game.string()));
	Initialize();

	ASSERT_EQ(m_browser.GetSelectedGame(), Fixtures::GetGameId("kingpin"));
	EXPECT_EQ(m_browser.GetLaunchOptions(Fixtures::GetGameId("rtcw")).size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
// A rejected desktop entry is warned about once per session, not once per change to the installs.
TEST_F(CBrowserTest, ChangingTheInstallsDoesNotScanAgain)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteDesktopEntry("kingpin-native.desktop", "[Desktop Entry]\nType=Link\n");
	Initialize();
	m_browser.AddInstall(Fixtures::GetGameId("kingpin"), Config::EInstallKind::Command, "steam -applaunch 38430");
	m_browser.Terminate();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, EditedCommandIsUsedAtOnce)
{
	Initialize();
	m_browser.SetInstallCommand(Fixtures::GetGameId("kingpin"), 1, "steam -applaunch 38430");

	ASSERT_TRUE(m_browser.GetJoinLauncher(Fixtures::GetGameId("kingpin")).has_value());
	EXPECT_EQ(m_browser.GetJoinLauncher(Fixtures::GetGameId("kingpin"))->argv, (std::vector<std::string>{ "steam", "-applaunch", "38430" }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, EditedCommandIsSavedOnTerminate)
{
	Initialize();
	m_browser.SetInstallCommand(Fixtures::GetGameId("kingpin"), 1, "steam -applaunch 38430");
	m_browser.Terminate();

	EXPECT_TRUE(ReadText(m_configDir / "config.json").contains("steam -applaunch 38430"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, AddedInstallIsSavedAtOnce)
{
	Initialize();
	m_browser.AddInstall(Fixtures::GetGameId("kingpin"), Config::EInstallKind::Command, "steam -applaunch 38430");

	EXPECT_TRUE(ReadText(m_configDir / "config.json").contains("steam -applaunch 38430"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, RemovingTheFirstInstallKeepsTheOthersInStep)
{
	Initialize();
	m_browser.AddInstall(Fixtures::GetGameId("kingpin"), Config::EInstallKind::Command, "steam -applaunch 38430");
	m_browser.RemoveInstall(Fixtures::GetGameId("kingpin"), 1);

	ASSERT_EQ(m_browser.GetInstallLaunchers(Fixtures::GetGameId("kingpin")).size(), 1u);
	EXPECT_EQ(m_browser.GetInstallLaunchers(Fixtures::GetGameId("kingpin")).front().id, "install:2");
	EXPECT_EQ(m_browser.GetSettings().games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].installs.front().id, 2u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, RenamedInstallRenamesTheJoinLauncher)
{
	Initialize();
	m_browser.SetInstallName(Fixtures::GetGameId("kingpin"), 1, "Native");

	ASSERT_TRUE(m_browser.GetJoinLauncher(Fixtures::GetGameId("kingpin")).has_value());
	EXPECT_EQ(m_browser.GetJoinLauncher(Fixtures::GetGameId("kingpin"))->name, "Native");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, RemovedInstallIsSavedAtOnce)
{
	Initialize();
	m_browser.RemoveInstall(Fixtures::GetGameId("kingpin"), 1);

	EXPECT_FALSE(ReadText(m_configDir / "config.json").contains("fake-game.sh"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, BrokenFolderInstallReportsItsError)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	Initialize();
	m_browser.AddInstall(Fixtures::GetGameId("kingpin"), Config::EInstallKind::Folder, (m_root / "no-such-folder").string());

	EXPECT_EQ(m_browser.ResolveLauncher(Fixtures::GetGameId("kingpin"), "install:2").error_or(Launch::ELaunchError::SpawnFailed), Launch::ELaunchError::BrokenInstallFolder);
	m_browser.Terminate();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, JoinStartsTheInstallItIsGiven)
{
	Query::SServerAddress const address{ 0xCB007107, 31510 };
	siginfo_t info{};

	ASSERT_TRUE(StartWithServer());
	m_browser.AddInstall(Fixtures::GetGameId("kingpin"), Config::EInstallKind::Command, std::format("\"{}\" second", m_game.string()));
	ASSERT_TRUE(m_browser.Join(address, {}, "install:2").has_value());
	ASSERT_EQ(::waitid(P_ALL, 0, &info, WEXITED | WNOWAIT), 0);
	m_browser.Update();

	EXPECT_EQ(ReadText(m_logsDir / "game-kingpin.log"), "second\n+connect\n203.0.113.7:31510\n");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, HiddenGameIsSavedAtOnce)
{
	Initialize();
	m_browser.SetGameListed(Fixtures::GetGameId("quake2"), false);

	EXPECT_FALSE(m_browser.GetSettings().games[static_cast<size_t>(Fixtures::GetGameId("quake2"))].isListed);
	EXPECT_TRUE(ReadText(m_configDir / "config.json").contains(R"("listed": false)"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CBrowserTest, LastListedGameStaysListed)
{
	CExpectedLog const expected{ BrowserChannel, 1, 0 };

	std::string hidden{};

	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		hidden += (game.key == "kingpin") ? std::string{} : std::format(R"({}"{}": {{ "listed": false }})", hidden.empty() ? "" : ", ", game.key);
	}

	WriteConfig(std::format(R"({{ "game": "kingpin", "games": {{ {} }} }})", hidden));
	Initialize();
	m_browser.SetGameListed(Fixtures::GetGameId("kingpin"), false);

	EXPECT_TRUE(m_browser.GetSettings().games[static_cast<size_t>(Fixtures::GetGameId("kingpin"))].isListed);
	m_browser.Terminate();
}
} // namespace
} // namespace Lkt::Browser
