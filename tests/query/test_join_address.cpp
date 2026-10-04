#include "query/game_definition.hpp"
#include "query/join_address.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <expected>
#include <optional>

namespace Lkt::Query
{
namespace
{
constexpr uint32_t Ip{ 0x2D5E3A3C };

//////////////////////////////////////////////////////////////////////////
SGameDefinition MakeGame(int32_t queryPortOffset)
{
	SGameDefinition game{};

	game.queryPortOffset = queryPortOffset;

	return game;
}

//////////////////////////////////////////////////////////////////////////
TEST(JoinAddress, WithoutAnOffsetIsTheQueryAddress)
{
	EXPECT_EQ(ToJoinAddress(MakeGame(0), SServerAddress{ Ip, 27960 }, std::nullopt), (SServerAddress{ Ip, 27960 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(JoinAddress, OffsetIsTakenFromTheQueryPort)
{
	EXPECT_EQ(ToJoinAddress(MakeGame(1), SServerAddress{ Ip, 7778 }, std::nullopt), (SServerAddress{ Ip, 7777 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(JoinAddress, ReplyJoinPortWinsOverTheOffset)
{
	EXPECT_EQ(ToJoinAddress(MakeGame(1), SServerAddress{ Ip, 7778 }, uint16_t{ 7000 }), (SServerAddress{ Ip, 7000 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(JoinAddress, OffsetLeavingThePortRangeKeepsTheQueryAddress)
{
	EXPECT_EQ(ToJoinAddress(MakeGame(1), SServerAddress{ Ip, 1 }, std::nullopt), (SServerAddress{ Ip, 1 }));
	EXPECT_EQ(ToJoinAddress(MakeGame(-1), SServerAddress{ Ip, 65535 }, std::nullopt), (SServerAddress{ Ip, 65535 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryAddress, OffsetIsAddedToTheJoinPort)
{
	EXPECT_EQ(ToQueryAddress(MakeGame(1), SServerAddress{ Ip, 7777 }), (SServerAddress{ Ip, 7778 }));
	EXPECT_EQ(ToQueryAddress(MakeGame(-10), SServerAddress{ Ip, 27015 }), (SServerAddress{ Ip, 27005 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryAddress, OffsetLeavingThePortRangeIsMalformed)
{
	EXPECT_EQ(ToQueryAddress(MakeGame(1), SServerAddress{ Ip, 65535 }), std::unexpected{ EParseError::Malformed });
	EXPECT_EQ(ToQueryAddress(MakeGame(-1), SServerAddress{ Ip, 1 }), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST(QueryAddress, JoinAddressComesBackFromIt)
{
	SGameDefinition const game{ MakeGame(-10) };
	SServerAddress const join{ Ip, 27015 };

	EXPECT_EQ(ToJoinAddress(game, ToQueryAddress(game, join).value(), std::nullopt), join);
}
} // namespace
} // namespace Lkt::Query
