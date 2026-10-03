#include "query/game_catalog.hpp"
#include <gtest/gtest.h>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(GameCatalog, FindsGameByKey)
{
	SGameDefinition const* const pGame{ FindGame("kingpin") };

	ASSERT_NE(pGame, nullptr);
	EXPECT_EQ(pGame->game, EGame::Kingpin);
}

//////////////////////////////////////////////////////////////////////////
TEST(GameCatalog, UnknownKeyFindsNothing)
{
	EXPECT_EQ(FindGame("doom"), nullptr);
}
} // namespace
} // namespace Lkt::Query
