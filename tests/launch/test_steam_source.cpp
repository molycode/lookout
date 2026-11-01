#include "channels.hpp"
#include "launch/launcher_ids.hpp"
#include "steam_source.hpp"
#include "temp_dir.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
namespace
{
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

constexpr uint32_t AppId{ 38430 };
constexpr std::filesystem::perms Executable{ std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read };

//////////////////////////////////////////////////////////////////////////
void WriteFile(std::filesystem::path const& path, std::string_view text)
{
	std::filesystem::create_directories(path.parent_path());

	std::ofstream file{ path, std::ios::binary };

	file << text;
}

//////////////////////////////////////////////////////////////////////////
class CSteamSourceTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-steam-source"));
		m_library = m_tempDir.GetPath() / "SteamLibrary";
		m_folder = m_library / "steamapps" / "common" / "Kingpin";
		m_environment.steamLibraries = { m_library };
		m_hints.onWindows = Query::SWindowsLaunchHints{ AppId, 0, {}, "kingpin.exe", { "main/pak0.pak" } };
		WriteFile(m_folder / "kingpin.exe", "");
		std::filesystem::permissions(m_folder / "kingpin.exe", Executable);
		WriteFile(m_folder / "main" / "pak0.pak", "");
		WriteManifest(m_library, "4", "Kingpin");
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	static void WriteManifest(std::filesystem::path const& library, std::string_view stateFlags, std::string_view installDir)
	{
		WriteFile(library / "steamapps" / std::format("appmanifest_{}.acf", AppId), std::format("\"AppState\"\n{{\n\t\"appid\"\t\t\"{}\"\n\t\"name\"\t\t\"Kingpin\"\n"
			"\t\"StateFlags\"\t\t\"{}\"\n\t\"installdir\"\t\t\"{}\"\n}}\n", AppId, stateFlags, installDir));
	}

	std::vector<SLaunchOption> Find() const
	{
		CSteamSource source{};

		source.Initialize(m_environment);

		std::vector<SLaunchOption> options{ source.Find(m_hints, "Kingpin: Life of Crime") };

		source.Terminate();

		return options;
	}

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_library{};
	std::filesystem::path m_folder{};
	SLaunchEnvironment m_environment{};
	Query::SLaunchHints m_hints{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamSourceTest, InstalledAppIsFound)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };
	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options.front(), (SLaunchOption{ std::string{ SteamLauncherId }, "Kingpin", m_folder.string(),
		{ std::filesystem::path{ m_folder / "kingpin.exe" }.make_preferred().string() }, m_folder.string() }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamSourceTest, AppNotFullyInstalledIsSkipped)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteManifest(m_library, "1026", "Kingpin");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamSourceTest, AppInASecondLibraryIsFound)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	m_environment.steamLibraries.insert(m_environment.steamLibraries.begin(), m_tempDir.GetPath() / "Steam");

	EXPECT_EQ(Find().size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamSourceTest, InstallDirThatIsNotAFolderNameIsRefused)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteManifest(m_library, "4", "..\\\\Kingpin");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamSourceTest, FolderMissingARequiredFileIsWarned)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	std::filesystem::remove(m_folder / "main" / "pak0.pak");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamSourceTest, GameWithoutASteamAppReadsNothing)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	m_hints.onWindows.steamAppId = 0;

	EXPECT_TRUE(Find().empty());
}
} // namespace
} // namespace Lkt::Launch
