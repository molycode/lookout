#include "read_index.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace Lkt::Download
{
namespace
{
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
} // namespace
} // namespace Lkt::Download
