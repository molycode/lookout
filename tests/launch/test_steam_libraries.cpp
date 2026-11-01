#include "channels.hpp"
#include "steam_libraries.hpp"
#include "temp_dir.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <filesystem>
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

//////////////////////////////////////////////////////////////////////////
// A path as Steam writes it in its lists, backslashes doubled.
std::string ToVdf(std::filesystem::path const& path)
{
	std::string text{};

	for (char const c : path.string())
	{
		text += (c == '\\') ? std::string{ "\\\\" } : std::string{ c };
	}

	return text;
}

//////////////////////////////////////////////////////////////////////////
class CSteamLibrariesTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-steam"));
		m_steam = m_tempDir.GetPath() / "Steam";
		m_library = m_tempDir.GetPath() / "SteamLibrary";
		std::filesystem::create_directories(m_steam / "steamapps");
		std::filesystem::create_directories(m_library / "steamapps");
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	void WriteList(std::string_view text) const
	{
		std::ofstream file{ m_steam / "steamapps" / "libraryfolders.vdf", std::ios::binary };

		file << text;
	}

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_steam{};
	std::filesystem::path m_library{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamLibrariesTest, NewListNamesEveryLibrary)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteList("\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + ToVdf(m_steam) + "\"\n\t\t\"apps\" { \"38430\" \"1\" }\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\""
		+ ToVdf(m_library) + "\"\n\t}\n}\n");

	EXPECT_EQ(ReadSteamLibraries(m_steam), (std::vector<std::filesystem::path>{ m_steam, m_library }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamLibrariesTest, OldListAddsTheSteamFolder)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteList("\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\t\"1600000000\"\n\t\"1\"\t\t\"" + ToVdf(m_library) + "\"\n}\n");

	EXPECT_EQ(ReadSteamLibraries(m_steam), (std::vector<std::filesystem::path>{ m_library, m_steam }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamLibrariesTest, MissingListIsTheSteamFolderAlone)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	EXPECT_EQ(ReadSteamLibraries(m_steam), (std::vector<std::filesystem::path>{ m_steam }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamLibrariesTest, BrokenListIsWarnedAndLeavesTheSteamFolder)
{
	CExpectedLog const expected{ LaunchChannel, 1, 0 };

	WriteList("\"libraryfolders\"\n{\n\t\"0\"\n");

	EXPECT_EQ(ReadSteamLibraries(m_steam), (std::vector<std::filesystem::path>{ m_steam }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CSteamLibrariesTest, NoSteamFolderIsNoLibraries)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	EXPECT_TRUE(ReadSteamLibraries({}).empty());
}
} // namespace
} // namespace Lkt::Launch
