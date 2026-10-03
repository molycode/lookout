#include "channels.hpp"
#include "hinted_discovery.hpp"
#include "launch/launcher_ids.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <array>
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

constexpr std::string_view DesktopId{ "kingpin-native.desktop" };
constexpr std::array<std::string_view, 1> DesktopFiles{ DesktopId };
constexpr std::array<std::string_view, 2> RequiredFiles{ "kingpin.x86", "main/pak0.pak" };
constexpr SLaunchHints Hints{ Query::EGame::Kingpin, DesktopFiles, "Games/Kingpin", "run-game.sh", RequiredFiles };
constexpr std::string_view GameName{ "Kingpin: Life of Crime" };

constexpr std::filesystem::perms Executable{ std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read };

//////////////////////////////////////////////////////////////////////////
void WriteFile(std::filesystem::path const& path, std::string_view text)
{
	std::filesystem::create_directories(path.parent_path());

	std::ofstream file{ path, std::ios::binary };

	file << text;
}

//////////////////////////////////////////////////////////////////////////
void WriteProgram(std::filesystem::path const& path)
{
	WriteFile(path, "#!/bin/sh\n");
	std::filesystem::permissions(path, Executable);
}

//////////////////////////////////////////////////////////////////////////
class CLaunchDiscoveryTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-launch-XXXXXX").string() };

		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_root = pattern;
		m_game = m_root / "games" / "Kingpin";
		m_program = m_game / "run-game.sh";
		m_environment.dataHome = m_root / "home" / ".local" / "share";
		m_environment.dataDirs = { m_root / "usr" / "share" };
		m_environment.home = m_root / "home";
		m_environment.searchPath = (m_root / "bin").string();
		std::filesystem::create_directories(m_environment.home);
		WriteProgram(m_program);
	}

	void TearDown() override
	{
		std::error_code error{};

		std::filesystem::remove_all(m_root, error);
	}
	// ~testing::Test

	void WriteEntry(std::filesystem::path const& dataDir, std::string_view keys) const
	{
		WriteFile(dataDir / "applications" / DesktopId, std::string{ "[Desktop Entry]\n" } + std::string{ keys });
	}

	std::string ValidKeys(std::string_view name) const
	{
		return "Type=Application\nName=" + std::string{ name } + "\nExec=\"" + m_program.string() + "\"\nPath=" + m_game.string() + "\n";
	}

	void WriteInstallFolder() const
	{
		std::filesystem::path const folder{ m_environment.home / Hints.installDir };

		WriteProgram(folder / Hints.program);
		WriteFile(folder / "kingpin.x86", "");
		WriteFile(folder / "main" / "pak0.pak", "");
	}

	std::vector<SLaunchOption> Find() const
	{
		return FindLaunchOptions(Hints, GameName, m_environment);
	}

	std::filesystem::path m_root;
	std::filesystem::path m_game;
	std::filesystem::path m_program;
	SLaunchEnvironment m_environment;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, EntryInDataHomeIsFound)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteEntry(m_environment.dataHome, ValidKeys("Kingpin"));

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options[0], (SLaunchOption{ std::string{ DesktopId }, "Kingpin", m_game.string(), { m_program.string() }, m_game.string() }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, DataHomeShadowsDataDirs)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteEntry(m_environment.dataHome, ValidKeys("Mine"));
	WriteEntry(m_environment.dataDirs.front(), ValidKeys("System"));

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options[0].name, "Mine");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, HiddenEntryHidesALowerOneSilently)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteEntry(m_environment.dataHome, "Hidden=true\n");
	WriteEntry(m_environment.dataDirs.front(), ValidKeys("System"));

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
// Read as a stream, a directory would throw, which without exceptions would end Lookout.
TEST_F(CLaunchDiscoveryTest, DesktopFileThatIsADirectoryIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	std::filesystem::create_directories(m_environment.dataHome / "applications" / DesktopId);

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, EntryWithoutTypeIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteEntry(m_environment.dataHome, "Name=Kingpin\nExec=\"" + m_program.string() + "\"\n");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, MissingTryExecIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteEntry(m_environment.dataHome, ValidKeys("Kingpin") + "TryExec=kingpin-is-not-installed\n");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, MissingProgramIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	std::filesystem::remove(m_program);
	WriteEntry(m_environment.dataHome, ValidKeys("Kingpin"));

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, ProgramThatIsADirectoryIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteEntry(m_environment.dataHome, "Type=Application\nExec=\"" + m_game.string() + "\"\n");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
// Root may run a file only when some execute bit is set, so this holds in the release container too.
TEST_F(CLaunchDiscoveryTest, NonExecutableProgramIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	std::filesystem::permissions(m_program, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
	WriteEntry(m_environment.dataHome, ValidKeys("Kingpin"));

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, RelativePathIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteEntry(m_environment.dataHome, "Type=Application\nExec=./run-game.sh\nPath=games/Kingpin\n");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, RelativeProgramResolvesAgainstPath)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteEntry(m_environment.dataHome, "Type=Application\nExec=./run-game.sh +set developer 1\nPath=" + m_game.string() + "\n");

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options[0].argv, (std::vector<std::string>{ m_program.string(), "+set", "developer", "1" }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, ProgramNameIsFoundOnTheSearchPath)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };
	std::filesystem::path const program{ m_root / "bin" / "kingpin" };

	WriteProgram(program);
	WriteEntry(m_environment.dataHome, "Type=Application\nExec=kingpin %U\n");

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options[0].argv, (std::vector<std::string>{ program.string() }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, CompleteInstallFolderFollowsTheDesktopEntries)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };
	std::filesystem::path const folder{ m_environment.home / Hints.installDir };

	WriteEntry(m_environment.dataHome, ValidKeys("Kingpin"));
	WriteInstallFolder();

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 2u);
	EXPECT_EQ(options[1], (SLaunchOption{ std::string{ InstallDirLauncherId }, std::string{ GameName }, "~/Games/Kingpin", { (folder / "run-game.sh").string() }, folder.string() }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, InstallFolderThatADesktopEntryStartsIsListedOnce)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };
	std::filesystem::path const folder{ m_environment.home / Hints.installDir };

	WriteInstallFolder();
	WriteEntry(m_environment.dataHome, "Type=Application\nName=Kingpin\nExec=\"" + (folder / Hints.program).string() + "\"\nPath=" + folder.string() + "\n");

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options.front().id, DesktopId);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, IncompleteInstallFolderIsRejected)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteInstallFolder();
	std::filesystem::remove(m_environment.home / Hints.installDir / "main" / "pak0.pak");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, AbsentInstallFolderIsSilent)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CLaunchDiscoveryTest, MissingHomeIsReported)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	m_environment.home.clear();

	EXPECT_TRUE(Find().empty());
}
} // namespace
} // namespace Lkt::Launch
