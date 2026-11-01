#include "games/folder_watcher.hpp"
#include "temp_dir.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace Lkt::Games
{
namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds SettleTime{ 100ms };
constexpr std::chrono::milliseconds Patience{ 5s };
// Long enough for any wake a change could still cause.
constexpr std::chrono::milliseconds Quiet{ SettleTime * 5 };

//////////////////////////////////////////////////////////////////////////
class CFolderWatcherTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-watch"));
		m_root = m_tempDir.GetPath();
		m_userDir = m_root / "data" / "lookout";
	}

	void TearDown() override
	{
		m_watcher.Terminate();
		m_tempDir.Terminate();
	}
	// ~testing::Test

	void Start()
	{
		ASSERT_TRUE(m_watcher.Initialize(m_userDir, SettleTime, [this]()
		{
			std::lock_guard const lock{ m_mutex };

			++m_numWakes;
			m_wake.notify_all();
		}));
	}

	void WriteFile(std::filesystem::path const& relativePath, std::string_view text) const
	{
		std::filesystem::path const path{ m_userDir / relativePath };
		std::error_code error{};

		std::filesystem::create_directories(path.parent_path(), error);

		std::ofstream file{ path, std::ios::binary };

		file << text;
	}

	bool WaitForWakes(int numWakes)
	{
		std::unique_lock lock{ m_mutex };

		return m_wake.wait_for(lock, Patience, [this, numWakes]() { return m_numWakes >= numWakes; });
	}

	int GetNumWakesAfterQuiet()
	{
		std::this_thread::sleep_for(Quiet);

		std::lock_guard const lock{ m_mutex };

		return m_numWakes;
	}

	Lkt::Fixtures::CTempDir m_tempDir;
	std::filesystem::path m_root;
	std::filesystem::path m_userDir;
	CFolderWatcher m_watcher;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	int m_numWakes{ 0 };
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderWatcherTest, BurstOfWritesWakesOnce)
{
	WriteFile("games/mygame/game.json", "{}");
	Start();

	for (int count{ 0 }; count < 5; ++count)
	{
		WriteFile("games/mygame/game.json", std::to_string(count));
	}

	EXPECT_TRUE(WaitForWakes(1));
	EXPECT_EQ(GetNumWakesAfterQuiet(), 1);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderWatcherTest, UserFolderMadeLaterIsWatchedDownToEachGame)
{
	Start();
	WriteFile("games/mygame/game.json", "{}");

	ASSERT_TRUE(WaitForWakes(1));

	WriteFile("games/mygame/game.json", "{ }");

	EXPECT_TRUE(WaitForWakes(2));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderWatcherTest, RemovedGameFolderWakes)
{
	std::error_code error{};

	WriteFile("games/mygame/game.json", "{}");
	Start();
	std::filesystem::remove_all(m_userDir / "games" / "mygame", error);

	EXPECT_TRUE(WaitForWakes(1));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderWatcherTest, DotFilesDoNotWake)
{
	WriteFile("games/mygame/game.json", "{}");
	Start();
	WriteFile("games/mygame/.game.json.swp", "swap");

	EXPECT_EQ(GetNumWakesAfterQuiet(), 0);
}

//////////////////////////////////////////////////////////////////////////
// What the loader does not read, beside or below what it does; on Windows the settings sit in the same folder.
TEST_F(CFolderWatcherTest, ChangesTheLoaderDoesNotReadDoNotWake)
{
	std::error_code error{};

	WriteFile("games/mygame/game.json", "{}");
	std::filesystem::create_directories(m_userDir / "games" / "mygame" / "sub", error);
	std::filesystem::create_directories(m_userDir / "downloaded" / "games" / "quake2", error);
	std::filesystem::create_directories(m_userDir / ".git" / "objects" / "ab", error);
	Start();
	WriteFile("config.json", "{}");
	WriteFile("games/mygame/sub/notes.txt", "notes");
	WriteFile("downloaded/games/quake2/game.json", "{}");
	WriteFile(".git/objects/ab/cdef", "object");

	EXPECT_EQ(GetNumWakesAfterQuiet(), 0);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderWatcherTest, RemovedUserFolderWakesAndIsWatchedAgain)
{
	std::error_code error{};

	WriteFile("games/mygame/game.json", "{}");
	Start();
	std::filesystem::remove_all(m_userDir, error);

	ASSERT_TRUE(WaitForWakes(1));

	WriteFile("games/mygame/game.json", "{}");

	EXPECT_TRUE(WaitForWakes(2));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderWatcherTest, UserFolderUnderAMissingDotFolderIsWatchedOnceMade)
{
	m_userDir = m_root / ".local" / "share" / "lookout";
	Start();
	WriteFile("games/mygame/game.json", "{}");

	EXPECT_TRUE(WaitForWakes(1));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderWatcherTest, WatchingCreatesNothing)
{
	Start();
	m_watcher.Terminate();

	EXPECT_FALSE(std::filesystem::exists(m_root / "data"));
}
} // namespace
} // namespace Lkt::Games
