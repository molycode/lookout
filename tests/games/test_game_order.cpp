#include "game_order.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <string>
#include <vector>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(GameOrder, ReadsKeysInOrder)
{
	std::expected<std::vector<std::string>, std::string> const order{ ReadGameOrder("// sidebar order\n[ \"b\", \"a\" ]") };

	ASSERT_TRUE(order.has_value()) << order.error();
	EXPECT_EQ(*order, (std::vector<std::string>{ "b", "a" }));
}

//////////////////////////////////////////////////////////////////////////
TEST(GameOrder, KeyListedTwiceIsRejected)
{
	EXPECT_EQ(ReadGameOrder("[ \"a\", \"a\" ]").error_or(""), "'a' is listed twice");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameOrder, EntryThatIsNotAKeyIsRejected)
{
	EXPECT_EQ(ReadGameOrder("[ \"a\", 5 ]").error_or(""), "every entry must be a game key");
}

//////////////////////////////////////////////////////////////////////////
TEST(GameOrder, ObjectIsRejected)
{
	EXPECT_EQ(ReadGameOrder("{ \"order\": [] }").error_or(""), "it must hold an array of game keys");
}
} // namespace
} // namespace Lkt::Games
