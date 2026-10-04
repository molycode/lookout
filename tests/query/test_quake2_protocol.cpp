#include "fixtures.hpp"
#include "query/game_definition.hpp"
#include "query/protocol.hpp"
#include <gtest/gtest.h>
#include <algorithm>

namespace Lkt::Query
{
namespace
{
using Fixtures::LoadFixture;
using Fixtures::ToBytes;

constexpr size_t HeaderSize{ 12 };

//////////////////////////////////////////////////////////////////////////
IProtocol const& Quake2()
{
	return GetProtocol(EProtocolFamily::Quake2);
}

//////////////////////////////////////////////////////////////////////////
std::expected<SStatusReply, EParseError> ParseStatus(std::string_view datagram)
{
	return Quake2().ParseStatusReply(ToBytes(datagram));
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, AsksMastersWithPlainQuery)
{
	EXPECT_EQ(Quake2().MasterRequest(Fixtures::GetGameByKey("kingpin")), ToBytes("query"));
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, AsksServersForStatus)
{
	EXPECT_EQ(Quake2().StatusRequest(), ToBytes("\xFF\xFF\xFF\xFFstatus\n"));
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, ReadsKingpinMasterList)
{
	std::vector<std::byte> const datagram{ LoadFixture("kingpin/master-master.kingpin.info-0.bin") };
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(Quake2().ParseMasterReply(datagram, servers).has_value());
	EXPECT_EQ(servers.size(), (datagram.size() - HeaderSize) / 6);
	EXPECT_TRUE(std::ranges::contains(servers, SServerAddress{ 0x5DE252A5, 31510 }));
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, ReadsMasterListAfterSpaceSeparator)
{
	std::vector<std::byte> const datagram{ LoadFixture("quake2/master-master.quakeservers.net-0.bin") };
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(Quake2().ParseMasterReply(datagram, servers).has_value());
	EXPECT_EQ(servers.size(), (datagram.size() - HeaderSize) / 6);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, AcceptsEmptyMasterList)
{
	std::vector<SServerAddress> servers{};

	EXPECT_TRUE(Quake2().ParseMasterReply(ToBytes("\xFF\xFF\xFF\xFFservers\n"), servers).has_value());
	EXPECT_TRUE(servers.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, RejectsMasterReplyWithoutSeparator)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(Quake2().ParseMasterReply(ToBytes("\xFF\xFF\xFF\xFFservers"), servers), std::unexpected{ EParseError::WrongHeader });
	EXPECT_EQ(Quake2().ParseMasterReply(ToBytes("\xFF\xFF\xFF\xFFserversX"), servers), std::unexpected{ EParseError::WrongHeader });
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, KeepsCompleteEntriesBeforeTruncation)
{
	std::vector<std::byte> datagram{ LoadFixture("kingpin/master-master.kingpin.info-0.bin") };
	std::vector<SServerAddress> servers{};

	datagram.pop_back();

	EXPECT_EQ(Quake2().ParseMasterReply(datagram, servers), std::unexpected{ EParseError::Truncated });
	EXPECT_EQ(servers.size(), (datagram.size() - HeaderSize) / 6);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, RejectsQuake3MasterReply)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(Quake2().ParseMasterReply(LoadFixture("rtcw/master-wolfmaster.idsoftware.com-0.bin"), servers), std::unexpected{ EParseError::WrongHeader });
	EXPECT_TRUE(servers.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, ReadsKingpinStatus)
{
	std::expected<SStatusReply, EParseError> const reply{ Quake2().ParseStatusReply(LoadFixture("kingpin/status-93.226.82.165_31510.bin")) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(FindRule(reply.value(), "hostname"), "BM - kp.satoki.org");
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, ReadsEveryCapturedStatus)
{
	for (std::string_view const game : { "kingpin", "quake2" })
	{
		for (std::filesystem::path const& path : Fixtures::ListFixtures(game, "status-"))
		{
			std::expected<SStatusReply, EParseError> const reply{ Quake2().ParseStatusReply(LoadFixture(path.string())) };

			EXPECT_TRUE(reply.has_value() && reply->numMalformedPlayerLines == 0) << path;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, ReadsPlayerScorePingAndName)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n-3 81 \"Big Joe\"\n") };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->players[0].score, -3);
	EXPECT_EQ(reply->players[0].ping, 81u);
	EXPECT_EQ(reply->players[0].name, "Big Joe");
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, CountsUnreadablePlayerLine)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n0 50 \"ok\"\nnot a player\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, CountsPlayerWithUnterminatedName)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n0 50 \"open\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, CountsPlayerWithoutPing)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n5 \"name\"\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, ReadsInfoLineWithoutNewline)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(FindRule(reply.value(), "hostname"), "x");
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, RejectsRuleWithoutValue)
{
	EXPECT_EQ(ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\\mapname\n"), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, RejectsStatusWithOnlyTheHeader)
{
	EXPECT_EQ(ParseStatus("\xFF\xFF\xFF\xFFprint\n"), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST(Quake2Protocol, RejectsQuake3StatusReply)
{
	EXPECT_EQ(Quake2().ParseStatusReply(LoadFixture("rtcw/status-104.153.105.209_27960.bin")), std::unexpected{ EParseError::WrongHeader });
}
} // namespace
} // namespace Lkt::Query
