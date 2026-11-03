#include "loader_input.hpp"
#include <gtest/gtest.h>
#include <filesystem>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(LoaderInput, GamesProtocolsAndTheirFoldersCount)
{
	EXPECT_TRUE(IsLoaderInput("games"));
	EXPECT_TRUE(IsLoaderInput("games/mygame"));
	EXPECT_TRUE(IsLoaderInput("games/mygame/game.json"));
	EXPECT_TRUE(IsLoaderInput("protocols"));
	EXPECT_TRUE(IsLoaderInput("protocols/quake2.lua"));
}

//////////////////////////////////////////////////////////////////////////
TEST(LoaderInput, StoresAndTheirFolderCount)
{
	EXPECT_TRUE(IsLoaderInput("stores"));
	EXPECT_TRUE(IsLoaderInput("stores/steam.json"));
	EXPECT_FALSE(IsLoaderInput("stores/steam/inside"));
}

//////////////////////////////////////////////////////////////////////////
TEST(LoaderInput, DownloadedCountsButNotWhatIsInside)
{
	EXPECT_TRUE(IsLoaderInput("downloaded"));
	EXPECT_FALSE(IsLoaderInput("downloaded/games"));
	EXPECT_FALSE(IsLoaderInput("downloaded/games/quake2/game.json"));
}

//////////////////////////////////////////////////////////////////////////
TEST(LoaderInput, DotEntriesDoNotCount)
{
	EXPECT_FALSE(IsLoaderInput("games/mygame/.game.json.swp"));
	EXPECT_FALSE(IsLoaderInput(".git/objects/ab/cdef"));
	EXPECT_FALSE(IsLoaderInput("games/.mygame.new"));
}

//////////////////////////////////////////////////////////////////////////
TEST(LoaderInput, OtherFilesDoNotCount)
{
	EXPECT_FALSE(IsLoaderInput("config.json"));
	EXPECT_FALSE(IsLoaderInput("logs/tge_2026-10-31.log"));
	EXPECT_FALSE(IsLoaderInput("games/mygame/sub/file.txt"));
	EXPECT_FALSE(IsLoaderInput("protocols/sub/file.lua"));
	EXPECT_FALSE(IsLoaderInput(""));
}
} // namespace
} // namespace Lkt::Games
