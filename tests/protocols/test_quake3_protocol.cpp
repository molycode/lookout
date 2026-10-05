#include "conversation_driver.hpp"
#include "fixtures.hpp"
#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include "script/protocol_script.hpp"
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <chrono>
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
constexpr std::string_view HexEntry{ "\\2D5E3A3C6D38" };
constexpr std::string_view EchoChallenge{ "\xFF\xFF\xFF\xFF" "echo \"echoResponse getstatus oo3KyTWe6F7YBlNw8K4htAikGv1O1wIUJlf8ysTEUrxGloYr338BJL1IQG7PrTSf\"" };
constexpr std::string_view EchoAnswer{ "\xFF\xFF\xFF\xFF" "echoResponse getstatus oo3KyTWe6F7YBlNw8K4htAikGv1O1wIUJlf8ysTEUrxGloYr338BJL1IQG7PrTSf" };
std::map<std::string, std::string> const HexOptions{ { "masterQuery", "24 empty full" }, { "masterEntries", "hex" } };

//////////////////////////////////////////////////////////////////////////
class CQuake3ProtocolTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::expected<void, std::string> const loaded{ m_script.Initialize("quake3", Fixtures::GetProtocolByName("quake3").source) };

		ASSERT_TRUE(loaded.has_value()) << loaded.error_or("");
	}

	void TearDown() override
	{
		m_script.Terminate();
	}
	// ~testing::Test

	std::expected<void, EParseError> ParseMaster(std::string_view entries, std::vector<SServerAddress>& servers)
	{
		return Fixtures::ReadMasterDatagram(m_script, MasterOptions(), ToBytes(std::string{ Header } + std::string{ entries }), servers);
	}

	std::expected<void, EParseError> ParseHexMaster(std::string_view entries, std::vector<SServerAddress>& servers)
	{
		return Fixtures::ReadMasterDatagram(m_script, HexOptions, ToBytes(std::string{ Header } + std::string{ entries }), servers);
	}

	std::expected<Script::SScriptAction, std::string> StartMaster(std::string_view masterQuery)
	{
		return Fixtures::StartOnce(m_script, Script::EConversationKind::Master, { { "masterQuery", std::string{ masterQuery } } });
	}

	std::map<std::string, std::string> const& MasterOptions() const
	{
		return Fixtures::GetGameByKey("quake3").protocolOptions;
	}

	// What a conversation of that kind starts by sending, for that game.
	std::vector<std::vector<std::byte>> GetFirstSend(Script::EConversationKind kind, std::string_view game)
	{
		std::expected<Script::SScriptAction, std::string> const started{ Fixtures::StartOnce(m_script, kind, Fixtures::GetGameByKey(game).protocolOptions) };

		EXPECT_TRUE(started.has_value()) << started.error_or("");

		return started.has_value() ? started->send : std::vector<std::vector<std::byte>>{};
	}

	Script::CProtocolScript m_script;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, AsksMastersWithTheGamesProtocolNumber)
{
	EXPECT_EQ(GetFirstSend(Script::EConversationKind::Master, "et"), std::vector<std::vector<std::byte>>{ ToBytes("\xFF\xFF\xFF\xFFgetservers 84 empty full") });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, AsksForEachProtocolVersionInTurn)
{
	std::expected<Script::SScriptAction, std::string> const started{ StartMaster("15,16 empty full") };

	ASSERT_TRUE(started.has_value()) << started.error_or("");
	EXPECT_EQ(started->send, (std::vector<std::vector<std::byte>>{ ToBytes("\xFF\xFF\xFF\xFFgetservers 15 empty full"), ToBytes("\xFF\xFF\xFF\xFFgetservers 16 empty full") }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, ProtocolVersionsMayFollowTheGameName)
{
	std::expected<Script::SScriptAction, std::string> const started{ StartMaster("Tremulous 70,71") };

	ASSERT_TRUE(started.has_value()) << started.error_or("");
	EXPECT_EQ(started->send, (std::vector<std::vector<std::byte>>{ ToBytes("\xFF\xFF\xFF\xFFgetservers Tremulous 70"), ToBytes("\xFF\xFF\xFF\xFFgetservers Tremulous 71") }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, RefusesAnEmptyProtocolVersion)
{
	EXPECT_FALSE(StartMaster("15, empty full").has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, RefusesUnknownMasterEntries)
{
	EXPECT_FALSE(Fixtures::StartOnce(m_script, Script::EConversationKind::Master, { { "masterQuery", "24" }, { "masterEntries", "text" } }).has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, AsksServersForStatus)
{
	EXPECT_EQ(GetFirstSend(Script::EConversationKind::Server, "et"), std::vector<std::vector<std::byte>>{ ToBytes("\xFF\xFF\xFF\xFFgetstatus") });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, ReadsEntryAddressAndPort)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(Entry, servers).has_value());
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers[0], EntryAddress);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, EndMarkerWithPaddingEndsTheDatagram)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + std::string{ "\\EOT\0\0\0", 7 }, servers).has_value());
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, EndMarkerWithoutPaddingEndsTheDatagram)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + "\\EOT", servers).has_value());
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, LastEndMarkerEndsTheDatagram)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + "\\EOF", servers).has_value());
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, LoneBackslashEndsTheDatagram)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + "\\", servers).has_value());
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, SkipsWhatPrecedesTheFirstEntry)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ "\n\0 ", 3 } + std::string{ Entry }, servers).has_value());
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers[0], EntryAddress);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, ReadsHexEntries)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseHexMaster(std::string{ " " } + std::string{ HexEntry } + std::string{ HexEntry } + "\\EOT", servers).has_value());
	ASSERT_EQ(servers.size(), 2u);
	EXPECT_EQ(servers[0], EntryAddress);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, RejectsHexEntryWithAnotherCharacter)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ParseHexMaster(std::string{ HexEntry } + "\\2D5E3A3C6D3G", servers), std::unexpected{ EParseError::Malformed });
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, EndMarkerLeavesTheEndToTheQuietPeriod)
{
	std::expected<Script::SScriptAction, std::string> const action{ Fixtures::ReceiveOnce(m_script, Script::EConversationKind::Master, MasterOptions(),
		ToBytes(std::string{ Header } + std::string{ Entry } + "\\EOT")) };

	ASSERT_TRUE(action.has_value()) << action.error_or("");
	EXPECT_FALSE(action->isDone);
	EXPECT_EQ(action->quiet, std::chrono::milliseconds{ 1500 });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, AddressThatSpellsTheEndMarkerIsAServer)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ParseMaster(std::string{ Entry } + "\\EOT\x01\x6D\x38" + std::string{ Entry }, servers).has_value());
	EXPECT_EQ(servers.size(), 3u);
	EXPECT_TRUE(std::ranges::contains(servers, SServerAddress{ 0x454F5401, 27960 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, ReadsEveryCapturedMasterDatagram)
{
	for (std::string_view const game : { "rtcw", "et", "quake3", "cod4", "jk2", "tremulous" })
	{
		for (std::filesystem::path const& path : Fixtures::ListFixtures(game, "master-"))
		{
			std::vector<SServerAddress> servers{};

			EXPECT_TRUE(Fixtures::ReadMasterDatagram(m_script, MasterOptions(), LoadFixture(path.string()), servers).has_value() && !servers.empty()) << path;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, ReadsEveryCapturedHexMasterDatagram)
{
	for (std::filesystem::path const& path : Fixtures::ListFixtures("eliteforce", "master-"))
	{
		std::vector<SServerAddress> servers{};

		EXPECT_TRUE(Fixtures::ReadMasterDatagram(m_script, HexOptions, LoadFixture(path.string()), servers).has_value() && !servers.empty()) << path;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, KeepsCompleteEntriesBeforeTruncation)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ParseMaster(std::string{ Entry } + "\\\x2D\x5E\x3A", servers), std::unexpected{ EParseError::Truncated });
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, RejectsEntryWithoutSeparator)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ParseMaster("X\x2D\x5E\x3A\x3C\x6D\x38", servers), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, RejectsQuake2MasterReply)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(Fixtures::ReadMasterDatagram(m_script, MasterOptions(), LoadFixture("kingpin/master-master.kingpin.info-0.bin"), servers), std::unexpected{ EParseError::WrongHeader });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, ReadsEveryCapturedStatus)
{
	for (std::string_view const game : { "rtcw", "et", "quake3", "cod4", "jk2", "eliteforce", "tremulous" })
	{
		for (std::filesystem::path const& path : Fixtures::ListFixtures(game, "status-"))
		{
			std::expected<SStatusReply, EParseError> const reply{ Fixtures::ReadStatusDatagram(m_script, LoadFixture(path.string())) };

			EXPECT_TRUE(reply.has_value() && !FindRule(reply.value(), "sv_hostname").empty() && !reply->players.empty()
				&& reply->numMalformedPlayerLines == 0) << path;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, PlayerStillConnectingHasNoPing)
{
	std::expected<SStatusReply, EParseError> const reply{ Fixtures::ReadStatusDatagram(m_script, ToBytes("\xFF\xFF\xFF\xFFstatusResponse\n\\sv_hostname\\x\n0 -1 \"Dusza\"\n")) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->players[0].name, "Dusza");
	EXPECT_FALSE(reply->players[0].ping.has_value());
	EXPECT_EQ(reply->numMalformedPlayerLines, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, EchoesTheChallenge)
{
	std::expected<Script::SScriptAction, std::string> const action{ Fixtures::ReceiveOnce(m_script, Script::EConversationKind::Server, {}, ToBytes(EchoChallenge)) };

	ASSERT_TRUE(action.has_value()) << action.error_or("");
	EXPECT_EQ(action->send, std::vector<std::vector<std::byte>>{ ToBytes(EchoAnswer) });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, StatusAfterTheChallengeIsTheReply)
{
	std::vector<std::vector<std::byte>> const datagrams{ ToBytes(EchoChallenge), ToBytes(EchoChallenge), LoadFixture("jk2/status-91.120.101.92_28070.bin") };
	std::expected<SStatusReply, EParseError> const reply{ Fixtures::ReadStatusDatagrams(m_script, datagrams) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_FALSE(FindRule(reply.value(), "sv_hostname").empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CQuake3ProtocolTest, RejectsQuake2StatusReply)
{
	EXPECT_EQ(Fixtures::ReadStatusDatagram(m_script, LoadFixture("kingpin/status-93.226.82.165_31510.bin")), std::unexpected{ EParseError::WrongHeader });
}
} // namespace
} // namespace Lkt::Query
