#include "channels.hpp"
#include "hinted_discovery.hpp"
#include "launch/launcher_ids.hpp"
#include "temp_dir.hpp"
#include "query/store_id.hpp"
#include <tge/platform.hpp>
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
// Steam, GOG and the usual folders are where Windows keeps its games; Linux looks elsewhere.
#if defined(TGE_PLATFORM_WINDOWS)
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

constexpr std::string_view GameName{ "Kingpin: Life of Crime" };

//////////////////////////////////////////////////////////////////////////
void WriteFile(std::filesystem::path const& path, std::string_view text)
{
	std::filesystem::create_directories(path.parent_path());

	std::ofstream file{ path, std::ios::binary };

	file << text;
}

//////////////////////////////////////////////////////////////////////////
void WriteGame(std::filesystem::path const& folder)
{
	WriteFile(folder / "kingpin.exe", "");
	WriteFile(folder / "main" / "pak0.pak", "");
}

//////////////////////////////////////////////////////////////////////////
class CWindowsDiscoveryTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-windows-discovery"));
		m_library = m_tempDir.GetPath() / "SteamLibrary";
		m_steamFolder = m_library / "steamapps" / "common" / "Kingpin";
		m_gogFolder = m_tempDir.GetPath() / "GOG Games" / "Kingpin";
		m_environment.steamLibraries = { m_library };
		m_environment.gogInstalls = { SGogInstall{ 1207658912, m_gogFolder, "Kingpin (GOG)" } };
		m_environment.systemDrive = m_tempDir.GetPath();
		m_hints.onWindows = Query::SWindowsLaunchHints{ { Query::SInstallDirCandidate{ Query::EInstallDirBase::SystemDrive, "Kingpin" } }, "kingpin.exe",
			{ "main/pak0.pak" } };
		m_hints.stores = { Query::SStoreId{ "steam", "38430" }, Query::SStoreId{ "gog", "1207658912" } };
		WriteFile(m_library / "steamapps" / "appmanifest_38430.acf", "\"AppState\" { \"name\" \"Kingpin\" \"StateFlags\" \"4\" \"installdir\" \"Kingpin\" }");
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	std::vector<std::string> FindIds() const
	{
		std::vector<std::string> ids{};

		for (SLaunchOption const& option : FindLaunchOptions(m_hints, GameName, m_environment, {}))
		{
			ids.emplace_back(option.id);
		}

		return ids;
	}

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_library{};
	std::filesystem::path m_steamFolder{};
	std::filesystem::path m_gogFolder{};
	SLaunchEnvironment m_environment{};
	Query::SLaunchHints m_hints{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CWindowsDiscoveryTest, SteamComesBeforeGogAndFolders)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteGame(m_steamFolder);
	WriteGame(m_gogFolder);
	WriteGame(m_tempDir.GetPath() / "Kingpin");

	EXPECT_EQ(FindIds(), (std::vector<std::string>{ std::string{ SteamLauncherId }, std::string{ GogLauncherId }, std::format("{}:1", InstallDirLauncherId) }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CWindowsDiscoveryTest, FolderThatSteamHoldsIsListedOnce)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteGame(m_steamFolder);
	m_environment.systemDrive = m_library / "steamapps" / "common";
	m_hints.stores = { Query::SStoreId{ "steam", "38430" } };

	EXPECT_EQ(FindIds(), (std::vector<std::string>{ std::string{ SteamLauncherId } }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CWindowsDiscoveryTest, GameWithoutWindowsFactsFindsNothing)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteGame(m_steamFolder);
	m_hints.onWindows = Query::SWindowsLaunchHints{};

	EXPECT_TRUE(FindIds().empty());
}
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace
} // namespace Lkt::Launch
