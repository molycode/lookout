#include "conversation_driver.hpp"
#include "fixtures.hpp"
#include "query/protocol_definition.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include "script/protocol_script.hpp"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Lkt::Query
{
namespace
{
using Fixtures::LoadFixture;
using Fixtures::ToBytes;

constexpr std::string_view StatusPrefix{ "status-" };
// The engine hands a stream to the script in reads of at most this size.
constexpr size_t EngineReadSize{ 16u << 10 };
// The game's fixture folder and its name on the masters.
constexpr std::array<std::pair<std::string_view, std::string_view>, 6> Games{ {
	{ "ut99", "ut" }, { "unreal", "unreal" }, { "mohaa", "mohaa" }, { "bf1942", "bfield1942" }, { "rune", "rune" }, { "deusex", "deusex" }
} };
constexpr std::string_view Challenge{ "\\basic\\\\secure\\WVNJYU" };
// gsmsalg of WVNJYU under the gspylite key, as tools/query.py computes it and both masters accept.
constexpr std::string_view ChallengeAnswer{ "\\gamename\\gspylite\\location\\0\\validate\\FL2fKXOg\\final\\\\list\\\\gamename\\ut\\final\\" };

//////////////////////////////////////////////////////////////////////////
std::map<std::string, std::string> MakeOptions(std::string_view masterGame)
{
	return { { "masterGame", std::string{ masterGame } } };
}

//////////////////////////////////////////////////////////////////////////
// Each captured server's datagrams in the order they came.
std::vector<std::vector<std::vector<std::byte>>> LoadServers(std::string_view game)
{
	std::vector<std::vector<std::vector<std::byte>>> servers{};
	std::string previous{};

	for (std::filesystem::path const& path : Fixtures::ListFixtures(game, StatusPrefix))
	{
		std::string const file{ path.filename().string() };
		std::string const name{ file.substr(0, file.rfind('-')) };

		if (servers.empty() || name != previous)
		{
			servers.emplace_back();
			previous = name;
		}

		servers.back().emplace_back(LoadFixture(path.string()));
	}

	return servers;
}

//////////////////////////////////////////////////////////////////////////
class CGamespy1ProtocolTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::expected<void, std::string> const loaded{ m_script.Initialize("gamespy1", Fixtures::GetProtocolByName("gamespy1").source) };

		ASSERT_TRUE(loaded.has_value()) << loaded.error_or("");
	}

	void TearDown() override
	{
		m_script.Terminate();
	}
	// ~testing::Test

	std::expected<void, EParseError> ReadMaster(std::string_view masterGame, std::span<std::byte const> stream, size_t pieceSize, std::vector<SServerAddress>& servers)
	{
		return Fixtures::ReadMasterStream(m_script, MakeOptions(masterGame), stream, pieceSize, servers);
	}

	std::expected<SStatusReply, EParseError> ReadServer(std::initializer_list<std::string_view> datagrams)
	{
		std::vector<std::vector<std::byte>> bytes{};

		for (std::string_view const datagram : datagrams)
		{
			bytes.emplace_back(ToBytes(datagram));
		}

		return Fixtures::ReadStatusDatagrams(m_script, bytes);
	}

	Script::CProtocolScript m_script;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, AsksServersForStatus)
{
	std::expected<Script::SScriptAction, std::string> const started{ Fixtures::StartOnce(m_script, Script::EConversationKind::Server, {}) };

	ASSERT_TRUE(started.has_value()) << started.error_or("");
	EXPECT_EQ(started->send, std::vector<std::vector<std::byte>>{ ToBytes("\\status\\") });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, AnswersTheMastersChallenge)
{
	std::expected<Script::SScriptAction, std::string> const action{ Fixtures::ReceiveOnce(m_script, Script::EConversationKind::Master, MakeOptions("ut"), ToBytes(Challenge)) };

	ASSERT_TRUE(action.has_value()) << action.error_or("");
	EXPECT_EQ(action->send, std::vector<std::vector<std::byte>>{ ToBytes(ChallengeAnswer) });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, WaitsForTheWholeChallenge)
{
	std::expected<Script::SScriptAction, std::string> const action{ Fixtures::ReceiveOnce(m_script, Script::EConversationKind::Master, MakeOptions("ut"),
		ToBytes(Challenge.substr(0, Challenge.size() - 1))) };

	ASSERT_TRUE(action.has_value()) << action.error_or("");
	EXPECT_TRUE(action->send.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, ListsEveryServerOfEachCapturedMaster)
{
	constexpr std::array<std::pair<std::string_view, size_t>, 3> Masters{ { { "ut99/master-master.333networks.com-stream.bin", 1105 },
		{ "ut99/master-master.openspy.net-stream.bin", 495 }, { "bf1942/master-master.openspy.net-stream.bin", 89 } } };

	for (auto const& [path, numListed] : Masters)
	{
		std::vector<std::byte> const stream{ LoadFixture(path) };
		std::vector<SServerAddress> servers{};

		EXPECT_TRUE(ReadMaster(path.starts_with("bf1942") ? "bfield1942" : "ut", stream, EngineReadSize, servers).has_value()) << path;
		EXPECT_EQ(servers.size(), numListed) << path;
	}
}

//////////////////////////////////////////////////////////////////////////
// A stream may arrive cut anywhere, the challenge and the final marker included.
TEST_F(CGamespy1ProtocolTest, ReadsEveryCapturedMasterInSmallPieces)
{
	for (auto const& [game, masterGame] : Games)
	{
		for (std::filesystem::path const& path : Fixtures::ListFixtures(game, "master-"))
		{
			std::vector<std::byte> const stream{ LoadFixture(path.string()) };
			std::vector<SServerAddress> servers{};

			EXPECT_TRUE(ReadMaster(masterGame, stream, 5, servers).has_value() && !servers.empty()) << path;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, MarkerCutShortWaitsForTheRest)
{
	Script::SConversation conversation{ Script::EConversationKind::Master, 0 };

	ASSERT_TRUE(m_script.Start(conversation, MakeOptions("ut")).has_value());
	ASSERT_TRUE(m_script.Receive(conversation, ToBytes(Challenge)).has_value());

	std::expected<Script::SScriptAction, std::string> const action{ m_script.Receive(conversation, ToBytes("\\fin")) };

	m_script.End(conversation);
	ASSERT_TRUE(action.has_value()) << action.error_or("");
	EXPECT_FALSE(action->reason.has_value());
	EXPECT_FALSE(action->isDone);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, EntryListsTheQueryPort)
{
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ReadMaster("ut", ToBytes(std::string{ Challenge } + "\\ip\\208.102.42.157:7778\\final\\"), 1024, servers).has_value());
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers[0], (SServerAddress{ 0xD0662A9D, 7778 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, StreamEndingBeforeTheFinalMarkerIsTruncated)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ReadMaster("ut", ToBytes(std::string{ Challenge } + "\\ip\\208.102.42.157:7778"), 1024, servers), std::unexpected{ EParseError::Truncated });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, RejectsAnAddressWithThreeParts)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ReadMaster("ut", ToBytes(std::string{ Challenge } + "\\ip\\208.102.42:7778\\final\\"), 1024, servers), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, RefusalIsMalformed)
{
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ReadMaster("ut", ToBytes(std::string{ Challenge } + "\\fatal\\1\\error\\Validation error\\final\\"), 1024, servers), std::unexpected{ EParseError::Malformed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, ReadsEveryCapturedServer)
{
	for (auto const& [game, masterGame] : Games)
	{
		for (std::vector<std::vector<std::byte>> const& datagrams : LoadServers(game))
		{
			std::expected<SStatusReply, EParseError> const reply{ Fixtures::ReadStatusDatagrams(m_script, datagrams) };

			EXPECT_TRUE(reply.has_value() && !FindRule(reply.value(), "hostname").empty() && reply->joinPort.has_value()) << game;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, ReadsUnrealPlayers)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer({ "\\hostname\\x\\player_0\\Bob\\frags_0\\-5\\ping_0\\ 33\\team_0\\255\\final\\\\queryid\\7.1" }) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->players[0].name, "Bob");
	EXPECT_EQ(reply->players[0].score, -5);
	EXPECT_EQ(reply->players[0].ping, 33u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, ReadsBattlefieldPlayers)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer({ "\\hostname\\x\\deaths_0\\4\\kills_0\\71\\ping_0\\106\\playername_0\\Pal\\score_0\\71\\final\\" }) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->players[0].name, "Pal");
	EXPECT_EQ(reply->players[0].score, 71);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, PlayerFieldsAreNotRules)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer({ "\\hostname\\x\\player_0\\Bob\\final\\" }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_TRUE(FindRule(reply.value(), "player_0").empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, PasswordTrueOrFalseBecomesANumber)
{
	EXPECT_EQ(FindRule(ReadServer({ "\\hostname\\x\\password\\True\\final\\" }).value_or(SStatusReply{}), "password"), "1");
	EXPECT_EQ(FindRule(ReadServer({ "\\hostname\\x\\password\\False\\final\\" }).value_or(SStatusReply{}), "password"), "0");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, JoinPortIsTheHostPort)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer({ "\\hostname\\x\\hostport\\7777\\final\\" }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->joinPort, 7777);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, FinalPartWaitsForTheOthers)
{
	std::expected<Script::SScriptAction, std::string> const action{ Fixtures::ReceiveOnce(m_script, Script::EConversationKind::Server, {},
		ToBytes("\\mapname\\DM-Deck16][\\queryid\\7.2\\final\\")) };

	ASSERT_TRUE(action.has_value()) << action.error_or("");
	EXPECT_FALSE(action->reply.has_value());
	EXPECT_TRUE(action->quiet.has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, PartsInAnyOrderMakeOneReply)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer({ "\\mapname\\DM-Deck16][\\queryid\\7.2\\final\\", "\\hostname\\x\\queryid\\7.1" }) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(FindRule(reply.value(), "hostname"), "x");
	EXPECT_EQ(FindRule(reply.value(), "mapname"), "DM-Deck16][");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CGamespy1ProtocolTest, RejectsAReplyThatIsNotPairs)
{
	EXPECT_EQ(ReadServer({ "\xFF\xFF\xFF\xFFstatusResponse\n" }), std::unexpected{ EParseError::WrongHeader });
}
} // namespace
} // namespace Lkt::Query
