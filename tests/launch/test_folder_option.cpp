#include "channels.hpp"
#include "fixtures.hpp"
#include "launch/folder_option.hpp"
#include "query/game_definition.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
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
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-folder-XXXXXX").string() };

		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_folder = pattern;
		WriteFile(m_folder / "run-game.sh", "#!/bin/sh\n");
		std::filesystem::permissions(m_folder / "run-game.sh", Executable);
		WriteFile(m_folder / "kingpin.x86", "");
		WriteFile(m_folder / "main" / "pak0.pak", "");
	}

	void TearDown() override
	{
		std::error_code error{};

		std::filesystem::remove_all(m_folder, error);
	}
	// ~testing::Test

	std::filesystem::path m_folder;
};

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
TEST_F(CFolderOptionTest, GameWithoutHintsCannotStartFromAFolder)
{
	EXPECT_EQ(MakeFolderOption(Fixtures::GetGameByKey("quake2"), m_folder.string()).error_or(ELaunchError::SpawnFailed), ELaunchError::FolderInstallUnsupported);
}
} // namespace
} // namespace Lkt::Launch
