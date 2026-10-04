#include "games/check_folder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace Lkt::Games
{
namespace
{
// A PNG's signature and header chunk, enough to tell its size.
std::string MakePngHeader(uint32_t width, uint32_t height)
{
	std::string png{ "\x89PNG\r\n\x1A\n\0\0\0\x0DIHDR", 16 };

	for (uint32_t const value : { width, height })
	{
		for (int shift{ 24 }; shift >= 0; shift -= 8)
		{
			png += static_cast<char>((value >> shift) & 0xFF);
		}
	}

	return png + std::string(9, '\0');
}

//////////////////////////////////////////////////////////////////////////
// A folder laid out like lookout-games, holding its Kingpin as "mygame".
class CCheckFolderTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::filesystem::path const games{ LKT_LOOKOUT_GAMES_DIR };
		std::error_code error{};
		std::string pattern{ (std::filesystem::temp_directory_path(error) / "lookout-check-XXXXXX").string() };

		ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
		m_dir = pattern;
		std::filesystem::create_directories(m_dir / "protocols");
		std::filesystem::create_directories(m_dir / "games");
		std::filesystem::copy(games / "protocols" / "quake2.lua", m_dir / "protocols" / "quake2.lua", error);
		std::filesystem::copy(games / "games" / "kingpin", m_dir / "games" / "mygame", std::filesystem::copy_options::recursive, error);
		ASSERT_EQ(error.value(), 0) << error.message();
	}

	void TearDown() override
	{
		std::error_code error{};

		std::filesystem::remove_all(m_dir, error);
	}
	// ~testing::Test

	void WriteFile(std::filesystem::path const& relativePath, std::string_view text) const
	{
		std::ofstream file{ m_dir / relativePath, std::ios::binary };

		file << text;
		EXPECT_TRUE(file.good()) << relativePath;
	}

	// Its one problem, or an empty text for none or several.
	std::string CheckForOneProblem() const
	{
		SGameContent const content{ CheckGameFolder(m_dir) };

		EXPECT_EQ(content.problems.size(), 1u);

		return (content.problems.size() == 1) ? content.problems.front().text : std::string{};
	}

	std::filesystem::path m_dir;
};

//////////////////////////////////////////////////////////////////////////
TEST(CheckFolder, LookoutGamesPassesIt)
{
	SGameContent const content{ CheckGameFolder(LKT_LOOKOUT_GAMES_DIR) };

	EXPECT_TRUE(content.problems.empty()) << content.problems.front().text;
	EXPECT_FALSE(content.games.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CCheckFolderTest, IconWithoutItsLicenceIsAProblem)
{
	std::filesystem::remove(m_dir / "games" / "mygame" / "icon-licence.txt");

	EXPECT_TRUE(CheckForOneProblem().ends_with("/games/mygame/icon.png: has no icon-licence.txt beside it"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CCheckFolderTest, IconThatIsNoPngIsAProblem)
{
	WriteFile("games/mygame/icon.png", "GIF89a");

	EXPECT_TRUE(CheckForOneProblem().ends_with("/games/mygame/icon.png: is not a PNG"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CCheckFolderTest, IconThatIsNotSquareIsAProblem)
{
	WriteFile("games/mygame/icon.png", MakePngHeader(256, 128));

	EXPECT_TRUE(CheckForOneProblem().ends_with("/games/mygame/icon.png: is 256 x 128 px; an icon is square"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CCheckFolderTest, SmallIconIsAProblem)
{
	WriteFile("games/mygame/icon.png", MakePngHeader(64, 64));

	EXPECT_TRUE(CheckForOneProblem().ends_with("/games/mygame/icon.png: is 64 px; an icon is at least 128 px"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CCheckFolderTest, OtherFileInAGameIsAProblem)
{
	WriteFile("games/mygame/notes.txt", "mine");

	EXPECT_TRUE(CheckForOneProblem().ends_with("/games/mygame/notes.txt: is not a file a game holds"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CCheckFolderTest, FolderWithoutGamesOrProtocolsIsAProblem)
{
	SGameContent const content{ CheckGameFolder(m_dir / "games") };

	ASSERT_EQ(content.problems.size(), 1u);
	EXPECT_EQ(content.problems.front().text, "games: holds neither games/ nor protocols/");
}
} // namespace
} // namespace Lkt::Games
