#include "master_listing.hpp"
#include <gtest/gtest.h>

namespace Lkt::Ui
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(MasterListing, MasterWithoutAProtocolNamesTheGames)
{
	EXPECT_EQ(DescribeMasterListing("quake2", "", ""), "quake2 (the game's)");
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterListing, GameWithoutAProtocolYetIsStillTheGames)
{
	EXPECT_EQ(DescribeMasterListing("", "", ""), "The game's");
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterListing, MastersOwnProtocolIsNamed)
{
	EXPECT_EQ(DescribeMasterListing("quake2", "gamespy1", ""), "gamespy1");
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterListing, PortOffsetShowsItsSign)
{
	EXPECT_EQ(DescribeMasterListing("quake2", "gamespy1", "10"), "gamespy1 · +10");
}

//////////////////////////////////////////////////////////////////////////
TEST(MasterListing, NegativePortOffsetKeepsItsSign)
{
	EXPECT_EQ(DescribeMasterListing("quake2", "", "-10"), "quake2 (the game's) · -10");
}
} // namespace
} // namespace Lkt::Ui
