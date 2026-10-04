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
	EXPECT_EQ(pGame->key, "kingpin");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameCatalog, EachGameIsFoundByItsOwnId)
{
	for (SGameDefinition const& game : GetGameCatalog())
	{
		EXPECT_EQ(GetGame(game.game).key, game.key);
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(GameCatalog, GameNamesItsProtocol)
{
	SGameDefinition const* const pGame{ FindGame("quake3") };

	ASSERT_NE(pGame, nullptr);
	EXPECT_EQ(GetProtocol(pGame->protocol).name, "quake3");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameCatalog, UnknownKeyFindsNothing)
{
	EXPECT_EQ(FindGame("doom"), nullptr);
}
} // namespace
} // namespace Lkt::Query
