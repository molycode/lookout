#include "fixtures.hpp"
#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include "script/protocol_script.hpp"
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
class CQuake2ProtocolTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::expected<void, std::string> const loaded{ m_script.Initialize("quake2", Fixtures::GetProtocolByName("quake2").source) };

		ASSERT_TRUE(loaded.has_value()) << loaded.error_or("");
	}

	void TearDown() override
	{
		m_script.Terminate();
	}
	// ~testing::Test

	std::expected<SStatusReply, EParseError> ParseStatus(std::string_view datagram)
	{
		return m_script.ParseStatusReply(ToBytes(datagram));
	}

	Script::CProtocolScript m_script;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, AsksMastersWithPlainQuery)
{
	EXPECT_EQ(Fixtures::GetGameByKey("kingpin").masterRequest, ToBytes("query"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, AsksServersForStatus)
{
	EXPECT_EQ(Fixtures::GetGameByKey("kingpin").statusRequest, ToBytes("\xFF\xFF\xFF\xFFstatus\n"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, ReadsKingpinMasterList)
{
	std::vector<std::byte> const datagram{ LoadFixture("kingpin/master-master.kingpin.info-0.bin") };
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(m_script.ParseMasterReply(datagram, servers).has_value());
	EXPECT_EQ(servers.size(), (datagram.size() - HeaderSize) / 6);
	EXPECT_TRUE(std::ranges::contains(servers, SServerAddress{ 0x5DE252A5, 31510 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, ReadsMasterListAfterSpaceSeparator)
{
	std::vector<std::byte> const datagram{ LoadFixture("quake2/master-master.quakeservers.net-0.bin") };
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(m_script.ParseMasterReply(datagram, servers).has_value());
	EXPECT_EQ(servers.size(), (datagram.size() - HeaderSize) / 6);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, AcceptsEmptyMasterList)
{
	std::vector<SServerAddress> servers{};

	EXPECT_TRUE(m_script.ParseMasterReply(ToBytes("\xFF\xFF\xFF\xFFservers\n"), servers).has_value());
	EXPECT_TRUE(servers.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, RejectsMasterReplyWithoutSeparator)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(m_script.ParseMasterReply(ToBytes("\xFF\xFF\xFF\xFFservers"), servers), std::unexpected{ EParseError::WrongHeader });
	EXPECT_EQ(m_script.ParseMasterReply(ToBytes("\xFF\xFF\xFF\xFFserversX"), servers), std::unexpected{ EParseError::WrongHeader });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, KeepsCompleteEntriesBeforeTruncation)
{
	std::vector<std::byte> datagram{ LoadFixture("kingpin/master-master.kingpin.info-0.bin") };
	std::vector<SServerAddress> servers{};

	datagram.pop_back();

	EXPECT_EQ(m_script.ParseMasterReply(datagram, servers), std::unexpected{ EParseError::Truncated });
	EXPECT_EQ(servers.size(), (datagram.size() - HeaderSize) / 6);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, RejectsQuake3MasterReply)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(m_script.ParseMasterReply(LoadFixture("rtcw/master-wolfmaster.idsoftware.com-0.bin"), servers), std::unexpected{ EParseError::WrongHeader });
	EXPECT_TRUE(servers.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, ReadsKingpinStatus)
{
	std::expected<SStatusReply, EParseError> const reply{ m_script.ParseStatusReply(LoadFixture("kingpin/status-93.226.82.165_31510.bin")) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(FindRule(reply.value(), "hostname"), "BM - kp.satoki.org");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, ReadsEveryCapturedStatus)
{
	for (std::string_view const game : { "kingpin", "quake2" })
	{
		for (std::filesystem::path const& path : Fixtures::ListFixtures(game, "status-"))
		{
			std::expected<SStatusReply, EParseError> const reply{ m_script.ParseStatusReply(LoadFixture(path.string())) };

			EXPECT_TRUE(reply.has_value() && reply->numMalformedPlayerLines == 0) << path;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, ReadsPlayerScorePingAndName)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n-3 81 \"Big Joe\"\n") };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->players[0].score, -3);
	EXPECT_EQ(reply->players[0].ping, 81u);
	EXPECT_EQ(reply->players[0].name, "Big Joe");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, CountsUnreadablePlayerLine)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n0 50 \"ok\"\nnot a player\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, CountsPlayerWithUnterminatedName)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n0 50 \"open\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, CountsPlayerWithoutPing)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\n5 \"name\"\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, ReadsInfoLineWithoutNewline)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(FindRule(reply.value(), "hostname"), "x");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, RejectsRuleWithoutValue)
{
	EXPECT_EQ(ParseStatus("\xFF\xFF\xFF\xFFprint\n\\hostname\\x\\mapname\n"), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, RejectsStatusWithOnlyTheHeader)
{
	EXPECT_EQ(ParseStatus("\xFF\xFF\xFF\xFFprint\n"), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake2ProtocolTest, RejectsQuake3StatusReply)
{
	EXPECT_EQ(m_script.ParseStatusReply(LoadFixture("rtcw/status-104.153.105.209_27960.bin")), std::unexpected{ EParseError::WrongHeader });
}
} // namespace
} // namespace Lkt::Query
