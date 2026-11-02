#include "channels.hpp"
#include "fixtures.hpp"
#include "launch/folder_option.hpp"
#include "query/game_definition.hpp"
#include "temp_dir.hpp"
#include <tge/platform.hpp>
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Lkt::Launch
{
namespace
{
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

constexpr std::filesystem::perms Executable{ std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read };

//////////////////////////////////////////////////////////////////////////
void WriteFile(std::filesystem::path const& path, std::string_view text)
{
	std::filesystem::create_directories(path.parent_path());

	std::ofstream file{ path, std::ios::binary };

	file << text;
}

//////////////////////////////////////////////////////////////////////////
class CFolderOptionTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-folder"));
		m_folder = m_tempDir.GetPath();
		WriteFile(m_folder / "run-game.sh", "#!/bin/sh\n");
		std::filesystem::permissions(m_folder / "run-game.sh", Executable);
		WriteFile(m_folder / "kingpin.x86", "");
		WriteFile(m_folder / "main" / "pak0.pak", "");
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_folder{};
};

#if defined(TGE_PLATFORM_WINDOWS)
//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, CompleteFolderStartsItsWindowsProgram)
{
	WriteFile(m_folder / "kingpin.exe", "");

	std::expected<SLaunchOption, ELaunchError> const option{ MakeFolderOption(Fixtures::GetGameByKey("kingpin"), m_folder.string()) };

	ASSERT_TRUE(option.has_value());
	EXPECT_EQ(option->argv, (std::vector<std::string>{ (m_folder / "kingpin.exe").string() }));
	EXPECT_EQ(option->workingDir, m_folder.string());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, FolderMissingAWindowsFileIsBroken)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteFile(m_folder / "kingpin.exe", "");
	std::filesystem::remove(m_folder / "main" / "pak0.pak");

	EXPECT_EQ(MakeFolderOption(Fixtures::GetGameByKey("kingpin"), m_folder.string()).error_or(ELaunchError::SpawnFailed), ELaunchError::BrokenInstallFolder);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, ProgramInASubfolderStartsInItsFolder)
{
	Query::SGameDefinition game{ Fixtures::GetGameByKey("jka") };

	game.launch.onWindows = Query::SWindowsLaunchHints{ 6020, 0, {}, "GameData/jamp.exe", { "GameData/base/assets0.pk3" } };
	WriteFile(m_folder / "GameData" / "jamp.exe", "");
	WriteFile(m_folder / "GameData" / "base" / "assets0.pk3", "");

	std::expected<SLaunchOption, ELaunchError> const option{ MakeFolderOption(game, m_folder.string()) };

	ASSERT_TRUE(option.has_value());
	EXPECT_EQ(option->workingDir, (m_folder / "GameData").string());
}
#elif defined(TGE_PLATFORM_LINUX)
//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, CompleteFolderStartsItsProgram)
{
	std::expected<SLaunchOption, ELaunchError> const option{ MakeFolderOption(Fixtures::GetGameByKey("kingpin"), m_folder.string()) };

	ASSERT_TRUE(option.has_value());
	EXPECT_EQ(option->argv, (std::vector<std::string>{ (m_folder / "run-game.sh").string() }));
	EXPECT_EQ(option->workingDir, m_folder.string());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, FolderMissingAFileIsBroken)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	std::filesystem::remove(m_folder / "main" / "pak0.pak");

	EXPECT_EQ(MakeFolderOption(Fixtures::GetGameByKey("kingpin"), m_folder.string()).error_or(ELaunchError::SpawnFailed), ELaunchError::BrokenInstallFolder);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, ProgramThatIsNotExecutableIsBroken)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	std::filesystem::permissions(m_folder / "run-game.sh", std::filesystem::perms::owner_read);

	EXPECT_EQ(MakeFolderOption(Fixtures::GetGameByKey("kingpin"), m_folder.string()).error_or(ELaunchError::SpawnFailed), ELaunchError::BrokenInstallFolder);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, ProgramInASubfolderStartsInItsFolder)
{
	WriteFile(m_folder / "System" / "ut-bin-amd64", "");
	std::filesystem::permissions(m_folder / "System" / "ut-bin-amd64", Executable);
	WriteFile(m_folder / "System" / "Core.u", "");
	WriteFile(m_folder / "System" / "Engine.u", "");
	WriteFile(m_folder / "System" / "Default.ini", "");

	std::expected<SLaunchOption, ELaunchError> const option{ MakeFolderOption(Fixtures::GetGameByKey("ut99"), m_folder.string()) };

	ASSERT_TRUE(option.has_value());
	EXPECT_EQ(option->workingDir, (m_folder / "System").string());
}
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderOptionTest, GameWithoutHintsCannotStartFromAFolder)
{
	EXPECT_EQ(MakeFolderOption(Fixtures::GetGameByKey("quake2"), m_folder.string()).error_or(ELaunchError::SpawnFailed), ELaunchError::FolderInstallUnsupported);
}
} // namespace
} // namespace Lkt::Launch
