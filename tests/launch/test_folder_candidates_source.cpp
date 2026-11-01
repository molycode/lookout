#include "channels.hpp"
#include "folder_candidates_source.hpp"
#include "launch/launcher_ids.hpp"
#include "temp_dir.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
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
class CFolderCandidatesSourceTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-folder-candidates"));
		m_environment.programFiles = m_tempDir.GetPath() / "Program Files";
		m_environment.programFilesX86 = m_tempDir.GetPath() / "Program Files (x86)";
		m_environment.localAppData = m_tempDir.GetPath() / "Local";
		m_environment.systemDrive = m_tempDir.GetPath() / "C";
		m_hints.onWindows = Query::SWindowsLaunchHints{ 0, 0, {}, "kingpin.exe", { "main/pak0.pak" } };
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	static void WriteGame(std::filesystem::path const& folder)
	{
		WriteFile(folder / "kingpin.exe", "");
		std::filesystem::permissions(folder / "kingpin.exe", Executable);
		WriteFile(folder / "main" / "pak0.pak", "");
	}

	std::vector<SLaunchOption> Find() const
	{
		CFolderCandidatesSource source{};

		source.Initialize(m_environment);

		std::vector<SLaunchOption> options{ source.Find(m_hints, "Kingpin: Life of Crime") };

		source.Terminate();

		return options;
	}

	Lkt::Fixtures::CTempDir m_tempDir{};
	SLaunchEnvironment m_environment{};
	Query::SLaunchHints m_hints{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderCandidatesSourceTest, EveryPlaceholderUsesItsFolder)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };
	std::array<std::pair<Query::EInstallDirBase, std::filesystem::path>, 4> const bases{ {
		{ Query::EInstallDirBase::ProgramFiles, m_environment.programFiles },
		{ Query::EInstallDirBase::ProgramFilesX86, m_environment.programFilesX86 },
		{ Query::EInstallDirBase::LocalAppData, m_environment.localAppData },
		{ Query::EInstallDirBase::SystemDrive, m_environment.systemDrive }
	} };

	for (std::pair<Query::EInstallDirBase, std::filesystem::path> const& base : bases)
	{
		std::filesystem::path folder{ base.second / "Xatrix" / "Kingpin" };

		folder.make_preferred();
		WriteGame(folder);
		m_hints.onWindows.installDirs = { Query::SInstallDirCandidate{ base.first, "Xatrix/Kingpin" } };

		std::vector<SLaunchOption> const options{ Find() };

		ASSERT_EQ(options.size(), 1u);
		EXPECT_EQ(options.front().workingDir, folder.string());
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderCandidatesSourceTest, CandidatesKeepTheirOrderAndIds)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteGame(m_environment.systemDrive / "Kingpin");
	WriteGame(m_environment.programFilesX86 / "Kingpin");
	m_hints.onWindows.installDirs = { Query::SInstallDirCandidate{ Query::EInstallDirBase::ProgramFiles, "Kingpin" },
		Query::SInstallDirCandidate{ Query::EInstallDirBase::SystemDrive, "Kingpin" },
		Query::SInstallDirCandidate{ Query::EInstallDirBase::ProgramFilesX86, "Kingpin" } };

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 2u);
	EXPECT_EQ(options[0].id, std::format("{}:2", InstallDirLauncherId));
	EXPECT_EQ(options[1].id, std::format("{}:3", InstallDirLauncherId));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderCandidatesSourceTest, AbsentCandidateIsSilent)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	m_hints.onWindows.installDirs = { Query::SInstallDirCandidate{ Query::EInstallDirBase::SystemDrive, "Kingpin" } };

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderCandidatesSourceTest, CandidateMissingARequiredFileIsWarned)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteGame(m_environment.systemDrive / "Kingpin");
	std::filesystem::remove(m_environment.systemDrive / "Kingpin" / "main" / "pak0.pak");
	m_hints.onWindows.installDirs = { Query::SInstallDirCandidate{ Query::EInstallDirBase::SystemDrive, "Kingpin" } };

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CFolderCandidatesSourceTest, BaseThatCouldNotBeLocatedIsSkipped)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteGame(m_environment.localAppData / "Kingpin");
	m_environment.localAppData.clear();
	m_hints.onWindows.installDirs = { Query::SInstallDirCandidate{ Query::EInstallDirBase::LocalAppData, "Kingpin" } };

	EXPECT_TRUE(Find().empty());
}
} // namespace
} // namespace Lkt::Launch
