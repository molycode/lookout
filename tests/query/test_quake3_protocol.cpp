#include "fixtures.hpp"
#include "query/game_definition.hpp"
#include "query/protocol.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <string>

namespace Lkt::Query
{
namespace
{
using Fixtures::LoadFixture;
using Fixtures::ToBytes;

constexpr std::string_view Header{ "\xFF\xFF\xFF\xFFgetserversResponse" };
constexpr std::string_view Entry{ "\\\x2D\x5E\x3A\x3C\x6D\x38" };
constexpr SServerAddress EntryAddress{ 0x2D5E3A3C, 27960 };

//////////////////////////////////////////////////////////////////////////
IProtocol const& Quake3()
{
	return GetProtocol(EProtocolFamily::Quake3);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, EParseError> ParseMaster(std::string_view entries, std::vector<SServerAddress>& servers)
{
	return Quake3().ParseMasterReply(ToBytes(std::string{ Header } + std::string{ entries }), servers);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, AsksMastersWithTheGamesProtocolNumber)
{
	EXPECT_EQ(Quake3().MasterRequest(Fixtures::GetGameByKey("et")), ToBytes("\xFF\xFF\xFF\xFFgetservers 84 empty full"));
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, AsksServersForStatus)
{
	EXPECT_EQ(Quake3().StatusRequest(), ToBytes("\xFF\xFF\xFF\xFFgetstatus"));
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, ReadsEntryAddressAndPort)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(Entry, servers).has_value());
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers[0], EntryAddress);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, EndMarkerWithPaddingEndsTheDatagram)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + std::string{ "\\EOT\0\0\0", 7 }, servers).has_value());
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, EndMarkerWithoutPaddingEndsTheDatagram)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + "\\EOT", servers).has_value());
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, AddressThatSpellsTheEndMarkerIsAServer)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + "\\EOT\x01\x6D\x38" + std::string{ Entry }, servers).has_value());
	EXPECT_EQ(servers.size(), 3u);
	EXPECT_TRUE(std::ranges::contains(servers, SServerAddress{ 0x454F5401, 27960 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, ReadsEveryCapturedMasterDatagram)
{
	for (std::string_view const game : { "rtcw", "et", "quake3" })
	{
		for (std::filesystem::path const& path : Fixtures::ListFixtures(game, "master-"))
		{
			std::vector<SServerAddress> servers{};

			EXPECT_TRUE(Quake3().ParseMasterReply(LoadFixture(path.string()), servers).has_value() && !servers.empty()) << path;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, KeepsCompleteEntriesBeforeTruncation)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ParseMaster(std::string{ Entry } + "\\\x2D\x5E\x3A", servers), std::unexpected{ EParseError::Truncated });
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, RejectsEntryWithoutSeparator)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ParseMaster("X\x2D\x5E\x3A\x3C\x6D\x38", servers), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, RejectsQuake2MasterReply)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(Quake3().ParseMasterReply(LoadFixture("kingpin/master-master.kingpin.info-0.bin"), servers), std::unexpected{ EParseError::WrongHeader });
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, ReadsEveryCapturedStatus)
{
	for (std::string_view const game : { "rtcw", "et", "quake3" })
	{
		for (std::filesystem::path const& path : Fixtures::ListFixtures(game, "status-"))
		{
			std::expected<SStatusReply, EParseError> const reply{ Quake3().ParseStatusReply(LoadFixture(path.string())) };

			EXPECT_TRUE(reply.has_value() && !FindRule(reply.value(), "sv_hostname").empty() && !reply->players.empty()
				&& reply->numMalformedPlayerLines == 0) << path;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake3Protocol, RejectsQuake2StatusReply)
{
	EXPECT_EQ(Quake3().ParseStatusReply(LoadFixture("kingpin/status-93.226.82.165_31510.bin")), std::unexpected{ EParseError::WrongHeader });
}
} // namespace
} // namespace Lkt::Query
