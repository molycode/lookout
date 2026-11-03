#include "channels.hpp"
#include "gog_source.hpp"
#include "launch/launcher_ids.hpp"
#include "temp_dir.hpp"
#include "query/store_id.hpp"
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

constexpr uint64_t GogId{ 1207658912 };
constexpr std::filesystem::perms Executable{ std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read };

//////////////////////////////////////////////////////////////////////////
void WriteFile(std::filesystem::path const& path, std::string_view text)
{
	std::filesystem::create_directories(path.parent_path());

	std::ofstream file{ path, std::ios::binary };

	file << text;
}

//////////////////////////////////////////////////////////////////////////
class CGogSourceTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-gog-source"));
		m_folder = m_tempDir.GetPath() / "GOG Games" / "Kingpin";
		m_environment.gogInstalls = { SGogInstall{ GogId, m_folder, "Kingpin: Life of Crime (GOG)" } };
		m_hints.onWindows = Query::SWindowsLaunchHints{ {}, "kingpin.exe", { "main/pak0.pak" } };
		m_hints.stores = { Query::SStoreId{ "gog", std::format("{}", GogId) } };
		WriteFile(m_folder / "kingpin.exe", "");
		std::filesystem::permissions(m_folder / "kingpin.exe", Executable);
		WriteFile(m_folder / "main" / "pak0.pak", "");
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	std::vector<SLaunchOption> Find() const
	{
		CGogSource source{};

		source.Initialize(m_environment);

		std::vector<SLaunchOption> options{ source.Find(m_hints, "Kingpin: Life of Crime") };

		source.Terminate();

		return options;
	}

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_folder{};
	SLaunchEnvironment m_environment{};
	Query::SLaunchHints m_hints{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGogSourceTest, RegisteredGameIsFound)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };
	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options.front().id, GogLauncherId);
	EXPECT_EQ(options.front().name, "Kingpin: Life of Crime (GOG)");
	EXPECT_EQ(options.front().workingDir, m_folder.string());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGogSourceTest, UnregisteredGameIsSilent)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	m_hints.stores = { Query::SStoreId{ "gog", std::format("{}", GogId + 1) } };

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGogSourceTest, RegisteredFolderMissingTheProgramIsWarned)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	std::filesystem::remove(m_folder / "kingpin.exe");

	EXPECT_TRUE(Find().empty());
}
} // namespace
} // namespace Lkt::Launch
