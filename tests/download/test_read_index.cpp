#include "read_index.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace Lkt::Download
{
namespace
{
//////////////////////////////////////////////////////////////////////////
// An index of no games, with fields put after its commit.
std::string MakeEmptyIndex(std::string_view fields)
{
	return std::format(R"({{ "index": 1, "commit": "0123456789abcdef0123456789abcdef01234567", {} "games": {{}}, "protocols": {{}} }})", fields);
}

//////////////////////////////////////////////////////////////////////////
// Its files must fit the sizes Lookout allows, or the published index would be refused whole.
TEST(ReadIndex, LookoutGamesIndexReads)
{
	std::ifstream file{ std::filesystem::path{ LKT_LOOKOUT_GAMES_DIR } / "index.json", std::ios::binary };
	std::string const text{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
	std::expected<SGameIndex, std::string> const index{ ReadIndex(text) };

	ASSERT_TRUE(index.has_value()) << index.error();
	EXPECT_FALSE(index->games.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(ReadIndex, IndexNamesLookoutsLatestVersion)
{
	std::expected<SGameIndex, std::string> const index{ ReadIndex(MakeEmptyIndex(R"("lookoutVersion": "9.0.41",)")) };

	ASSERT_TRUE(index.has_value()) << index.error();
	EXPECT_EQ(index->lookoutVersion, "9.0.41");
}

//////////////////////////////////////////////////////////////////////////
TEST(ReadIndex, IndexMayNameNoLookoutVersion)
{
	std::expected<SGameIndex, std::string> const index{ ReadIndex(MakeEmptyIndex("")) };

	ASSERT_TRUE(index.has_value()) << index.error();
	EXPECT_TRUE(index->lookoutVersion.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(ReadIndex, LookoutVersionThatIsNoneRefusesTheIndex)
{
	EXPECT_EQ(ReadIndex(MakeEmptyIndex(R"("lookoutVersion": "v1.4.0",)")), std::unexpected{ std::string{ "index.json: lookoutVersion: must be a version such as 1.4.0" } });
}
} // namespace
} // namespace Lkt::Download
