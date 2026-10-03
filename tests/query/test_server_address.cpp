#include "query/server_address.hpp"
#include <gtest/gtest.h>
#include <array>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, ParsesDottedQuadAndPort)
{
	std::expected<SServerAddress, EParseError> const address{ ParseAddress("45.94.58.60:31512") };

	ASSERT_TRUE(address.has_value());
	EXPECT_EQ(address->ipv4, 0x2D5E3A3Cu);
	EXPECT_EQ(address->port, 31512u);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, FormatsDottedQuadAndPort)
{
	EXPECT_EQ(FormatAddress(SServerAddress{ 0xCBD4111B, 31211 }), "203.212.17.27:31211");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, LongestAddressFitsTheBuffer)
{
	std::array<char, MaxFormattedAddress> buffer{};

	EXPECT_EQ(FormatAddressTo(SServerAddress{ 0xFFFFFFFF, 65535 }, buffer), "255.255.255.255:65535");
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, KeyTurnsBackIntoTheAddress)
{
	SServerAddress const address{ 0xCBD4111B, 31211 };

	EXPECT_EQ(FromKey(ToKey(address)), address);
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, RejectsMissingPort)
{
	EXPECT_EQ(ParseAddress("1.2.3.4"), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, RejectsOctetAboveByte)
{
	EXPECT_EQ(ParseAddress("1.2.3.256:27960"), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, RejectsWrongNumberOfOctets)
{
	EXPECT_FALSE(ParseAddress("1.2.3:27960").has_value());
	EXPECT_FALSE(ParseAddress("1.2.3.4.5:27960").has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, RejectsPortOutsideRange)
{
	EXPECT_FALSE(ParseAddress("1.2.3.4:0").has_value());
	EXPECT_FALSE(ParseAddress("1.2.3.4:65536").has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, RejectsTrailingCharacters)
{
	EXPECT_FALSE(ParseAddress("1.2.3.4:27960x").has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, KeyDistinguishesPortsOnOneHost)
{
	EXPECT_NE(ToKey(SServerAddress{ 0x01020304, 31510 }), ToKey(SServerAddress{ 0x01020304, 31511 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, PublicServerIsQueryable)
{
	EXPECT_TRUE(IsQueryable(SServerAddress{ 0x2D5E3A3C, 27960 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, OwnNetworksAreNotQueryable)
{
	for (std::string_view const text : { "127.0.0.1:27960", "10.1.2.3:27960", "172.16.0.9:27960", "172.31.255.1:27960",
		"192.168.1.20:27960", "169.254.3.4:27960", "100.64.0.1:27960", "0.1.2.3:27960" })
	{
		std::expected<SServerAddress, EParseError> const address{ ParseAddress(text) };

		EXPECT_TRUE(address.has_value() && !IsQueryable(address.value())) << text;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, MulticastAndBroadcastAreNotQueryable)
{
	EXPECT_FALSE(IsQueryable(SServerAddress{ 0xE0000001, 27960 }));
	EXPECT_FALSE(IsQueryable(SServerAddress{ 0xFFFFFFFF, 27960 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, NeighboursOfPrivateRangesAreQueryable)
{
	EXPECT_TRUE(IsQueryable(SServerAddress{ 0xAC200001, 27960 }));
	EXPECT_TRUE(IsQueryable(SServerAddress{ 0x0B000001, 27960 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerAddress, PortZeroIsNotQueryable)
{
	EXPECT_FALSE(IsQueryable(SServerAddress{ 0x2D5E3A3C, 0 }));
}
} // namespace
} // namespace Lkt::Query
