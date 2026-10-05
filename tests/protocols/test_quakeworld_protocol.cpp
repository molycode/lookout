#include "conversation_driver.hpp"
#include "fixtures.hpp"
#include "query/protocol_definition.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include "script/protocol_script.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Query
{
namespace
{
using Fixtures::LoadFixture;
using Fixtures::ToBytes;

constexpr std::string_view Game{ "quakeworld" };
constexpr std::string_view StatusHeader{ "\xFF\xFF\xFF\xFFn\\hostname\\x\\map\\dm4\n" };

//////////////////////////////////////////////////////////////////////////
class CQuakeWorldProtocolTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::expected<void, std::string> const loaded{ m_script.Initialize("quakeworld", Fixtures::GetProtocolByName("quakeworld").source) };

		ASSERT_TRUE(loaded.has_value()) << loaded.error_or("");
	}

	void TearDown() override
	{
		m_script.Terminate();
	}
	// ~testing::Test

	std::expected<SStatusReply, EParseError> ParseStatus(std::string_view datagram)
	{
		return Fixtures::ReadStatusDatagram(m_script, ToBytes(datagram));
	}

	std::vector<std::vector<std::byte>> GetFirstSend(Script::EConversationKind kind)
	{
		std::expected<Script::SScriptAction, std::string> const started{ Fixtures::StartOnce(m_script, kind, {}) };

		EXPECT_TRUE(started.has_value()) << started.error_or("");

		return started.has_value() ? started->send : std::vector<std::vector<std::byte>>{};
	}

	Script::CProtocolScript m_script;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, AsksMastersWithC)
{
	EXPECT_EQ(GetFirstSend(Script::EConversationKind::Master), std::vector<std::vector<std::byte>>{ ToBytes("c\n") });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, AsksServersForPlayersAndSpectators)
{
	EXPECT_EQ(GetFirstSend(Script::EConversationKind::Server), std::vector<std::vector<std::byte>>{ ToBytes("\xFF\xFF\xFF\xFFstatus 23\n") });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, ReadsEveryCapturedMasterDatagram)
{
	for (std::filesystem::path const& path : Fixtures::ListFixtures(Game, "master-"))
	{
		std::vector<SServerAddress> servers{};

		EXPECT_TRUE(Fixtures::ReadMasterDatagram(m_script, {}, LoadFixture(path.string()), servers).has_value() && !servers.empty()) << path;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, ReadsEntryAddressAndPort)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(Fixtures::ReadMasterDatagram(m_script, {}, ToBytes("\xFF\xFF\xFF\xFF" "d\n\x87\x7D\xEA\xDC\x6B\x6C"), servers).has_value());
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers[0], (SServerAddress{ 0x877DEADC, 27500 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, KeepsCompleteEntriesBeforeTruncation)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(Fixtures::ReadMasterDatagram(m_script, {}, ToBytes("\xFF\xFF\xFF\xFF" "d\n\x87\x7D\xEA\xDC\x6B\x6C\x87\x7D"), servers), std::unexpected{ EParseError::Truncated });
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, ReadsEveryCapturedStatus)
{
	for (std::filesystem::path const& path : Fixtures::ListFixtures(Game, "status-"))
	{
		std::expected<SStatusReply, EParseError> const reply{ Fixtures::ReadStatusDatagram(m_script, LoadFixture(path.string())) };

		EXPECT_TRUE(reply.has_value() && !FindRule(reply.value(), "hostname").empty() && !reply->players.empty()
			&& reply->numMalformedPlayerLines == 0) << path;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, ReadsPlayerFragsPingAndName)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus(std::string{ StatusHeader } + "76 -2 3 56 \"Mega Lom\" \"base\" 1 4 \"red\"\n") };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->players[0].name, "Mega Lom");
	EXPECT_EQ(reply->players[0].score, -2);
	EXPECT_EQ(reply->players[0].ping, 56u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, LeavesSpectatorsOut)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus(std::string{ StatusHeader }
		+ "5 -9999 3 -25 \"\\s\\Bob\" \"\" 0 0\n7 -9999 1 25 \"\\s\\Ann\" \"\" 0 0\n8 -9999 1 -40 \"Cid\" \"\" 0 0\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_TRUE(reply->players.empty());
	EXPECT_EQ(reply->numMalformedPlayerLines, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, CountsALineWithoutItsNumbers)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus(std::string{ StatusHeader } + "76 0 3 \"Mega Lom\" \"base\" 1 4\n") };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, ToleratesATrailingNul)
{
	std::expected<SStatusReply, EParseError> const reply{ ParseStatus(std::string{ StatusHeader } + std::string{ "1 0 3 0 \"bot\" \"base\" 1 4\n\0", 26 }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->numMalformedPlayerLines, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuakeWorldProtocolTest, RejectsQuake2StatusReply)
{
	EXPECT_EQ(Fixtures::ReadStatusDatagram(m_script, LoadFixture("kingpin/status-93.226.82.165_31510.bin")), std::unexpected{ EParseError::WrongHeader });
}
} // namespace
} // namespace Lkt::Query
