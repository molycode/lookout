#include "conversation_driver.hpp"
#include "fixtures.hpp"
#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include "script/protocol_script.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Lkt::Query
{
namespace
{
using namespace std::string_view_literals;
using Fixtures::LoadFixture;

constexpr std::string_view Game{ "ut2004" };
constexpr std::string_view StatusPrefix{ "status-" };
constexpr std::string_view MasterStream{ "ut2004/master-ut2004master.333networks.com-stream.bin" };

//////////////////////////////////////////////////////////////////////////
// A captured server's datagrams in the order they came, by "<address>_<port>".
std::vector<std::vector<std::byte>> LoadServer(std::string_view name)
{
	std::vector<std::vector<std::byte>> datagrams{};

	for (std::filesystem::path const& path : Fixtures::ListFixtures(Game, std::format("{}{}-", StatusPrefix, name)))
	{
		datagrams.emplace_back(LoadFixture(path.string()));
	}

	EXPECT_FALSE(datagrams.empty()) << name;

	return datagrams;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::string> ListCapturedServers()
{
	std::vector<std::string> names{};

	for (std::filesystem::path const& path : Fixtures::ListFixtures(Game, StatusPrefix))
	{
		std::string const file{ path.filename().string() };
		std::string name{ file.substr(StatusPrefix.size(), file.rfind('-') - StatusPrefix.size()) };

		if (names.empty() || names.back() != name)
		{
			names.emplace_back(std::move(name));
		}
	}

	return names;
}

//////////////////////////////////////////////////////////////////////////
// A frame of the master's stream: its size in four bytes, little-endian, then the payload.
std::string MakeFrame(std::string_view payload)
{
	uint32_t const size{ static_cast<uint32_t>(payload.size()) };
	std::string frame{};

	for (uint32_t shift{ 0 }; shift < 32; shift += 8)
	{
		frame += static_cast<char>((size >> shift) & 0xFF);
	}

	return frame + std::string{ payload };
}

//////////////////////////////////////////////////////////////////////////
class CUnreal2ProtocolTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		std::expected<void, std::string> const loaded{ m_script.Initialize("unreal2", Fixtures::GetProtocolByName("unreal2").source) };

		ASSERT_TRUE(loaded.has_value()) << loaded.error_or("");
	}

	void TearDown() override
	{
		m_script.Terminate();
	}
	// ~testing::Test

	std::expected<void, EParseError> ReadMaster(std::span<std::byte const> stream, size_t pieceSize, std::vector<SServerAddress>& servers)
	{
		return Fixtures::ReadMasterStream(m_script, Fixtures::GetGameByKey(Game).protocolOptions, stream, pieceSize, servers);
	}

	std::expected<SStatusReply, EParseError> ReadServer(std::span<std::vector<std::byte> const> datagrams)
	{
		return Fixtures::ReadStatusDatagrams(m_script, datagrams);
	}

	Script::CProtocolScript m_script;
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, ListsEveryServerOfEachCapturedMaster)
{
	constexpr std::array<std::pair<std::string_view, size_t>, 2> Masters{ { { MasterStream, 590 },
		{ "ut2004/master-utmaster.openspy.net-stream.bin", 627 } } };

	for (auto const& [path, numListed] : Masters)
	{
		std::vector<std::byte> const stream{ LoadFixture(path) };
		std::vector<SServerAddress> servers{};

		EXPECT_TRUE(ReadMaster(stream, stream.size(), servers).has_value()) << path;
		EXPECT_EQ(servers.size(), numListed) << path;
	}
}

//////////////////////////////////////////////////////////////////////////
// The first entry names game port 7777 and query port 7778.
TEST_F(CUnreal2ProtocolTest, EntryListsTheQueryPort)
{
	std::vector<std::byte> const stream{ LoadFixture(MasterStream) };
	std::vector<SServerAddress> servers{};

	ASSERT_TRUE(ReadMaster(stream, stream.size(), servers).has_value());
	EXPECT_EQ(servers.front(), (SServerAddress{ 0x51CCAE13, 7778 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, StreamReadInSmallPiecesListsTheSameServers)
{
	std::vector<std::byte> const stream{ LoadFixture(MasterStream) };
	std::vector<SServerAddress> whole{};
	std::vector<SServerAddress> pieces{};

	ASSERT_TRUE(ReadMaster(stream, stream.size(), whole).has_value());
	ASSERT_TRUE(ReadMaster(stream, 3, pieces).has_value());
	EXPECT_EQ(pieces, whole);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, StreamEndingEarlyIsTruncated)
{
	std::vector<std::byte> const stream{ LoadFixture(MasterStream) };
	std::vector<SServerAddress> servers{};

	EXPECT_EQ(ReadMaster(std::span{ stream }.first(stream.size() / 2), stream.size(), servers), std::unexpected{ EParseError::Truncated });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, RefusedClientFailsTheScript)
{
	std::string const stream{ MakeFrame("\x05SEED\0"sv) + MakeFrame("\x07" "DENIED\0"sv) };
	std::expected<Script::SScriptAction, std::string> const action{ Fixtures::ReceiveOnce(m_script, Script::EConversationKind::Master,
		Fixtures::GetGameByKey(Game).protocolOptions, Fixtures::ToBytes(stream)) };

	EXPECT_TRUE(action.error_or("").contains("DENIED"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, ReadsEveryCapturedServer)
{
	std::vector<std::string> const names{ ListCapturedServers() };

	ASSERT_FALSE(names.empty());

	for (std::string const& name : names)
	{
		std::expected<SStatusReply, EParseError> const reply{ ReadServer(LoadServer(name)) };

		EXPECT_TRUE(reply.has_value() && !FindRule(*reply, "hostname").empty() && !FindRule(*reply, "map").empty() && reply->joinPort.has_value()) << name;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, WideNameKeepsItsColourCodes)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer(LoadServer("85.206.69.158_7778")) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(FindRule(*reply, "hostname"),
		"\x1BH@@(UtC) \x1B@@@Invasion \x1BH@@+ \x1B@@@Boss \x1BH@@Waves \x1B@@@Server \x1BH@@2026! \x1B\x10\x10\x10New \x1B```Version \x1B@@@2.0");
}

//////////////////////////////////////////////////////////////////////////
// Its spaces are Latin-1 no-break spaces, left for the game's text style to decode.
TEST_F(CUnreal2ProtocolTest, NarrowNamePassesThroughAsSent)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer(LoadServer("173.225.184.201_7778")) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(FindRule(*reply, "hostname"), "Monster\xA0Madness -\xA0RPG\xA0WoP\xA0Invasion");
}

//////////////////////////////////////////////////////////////////////////
// Some servers send a rule "Game Password" of their own, which is not the game's.
TEST_F(CUnreal2ProtocolTest, OnlyGamePasswordMarksAPassword)
{
	for (std::string const& name : ListCapturedServers())
	{
		std::expected<SStatusReply, EParseError> const reply{ ReadServer(LoadServer(name)) };

		ASSERT_TRUE(reply.has_value()) << name;
		EXPECT_EQ(FindRule(*reply, "password"), (name == "47.146.3.181_8778") ? "1" : "0") << name;
	}
}

//////////////////////////////////////////////////////////////////////////
// Three of its sixteen entries are a mutator's team labels and round count.
TEST_F(CUnreal2ProtocolTest, TeamLabelsAreNotPlayers)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer(LoadServer("185.107.97.16_7778")) };

	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->players.size(), 13u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, PlayerWithIdZeroAndAPingIsAPlayer)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer(LoadServer("74.91.113.85_7778")) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_FALSE(reply->players.empty());
	EXPECT_EQ(reply->players.back().name, "ReaperX");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, PlayerInNoTeamHasNoTeamField)
{
	std::expected<SStatusReply, EParseError> const reply{ ReadServer(LoadServer("185.107.97.16_7778")) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_GE(reply->players.size(), 3u);
	EXPECT_EQ(reply->players[2].name, "damn_GOOD_coffee");
	EXPECT_TRUE(reply->players[2].fields.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, InfoRulesLeadWhateverTheOrderOfPackets)
{
	std::vector<std::vector<std::byte>> datagrams{ LoadServer("173.225.184.201_7778") };

	std::ranges::reverse(datagrams);

	std::expected<SStatusReply, EParseError> const reply{ ReadServer(datagrams) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_FALSE(reply->rules.empty());
	EXPECT_EQ(reply->rules.front().key, "hostname");
}

//////////////////////////////////////////////////////////////////////////
// A resent query is answered with the same packets again.
TEST_F(CUnreal2ProtocolTest, RepeatedPacketIsReadOnce)
{
	std::vector<std::vector<std::byte>> const once{ LoadServer("185.107.97.16_7778") };
	std::vector<std::vector<std::byte>> twice{ once };

	twice.insert(twice.end(), once.begin(), once.end());

	std::expected<SStatusReply, EParseError> const fromOnce{ ReadServer(once) };
	std::expected<SStatusReply, EParseError> const fromTwice{ ReadServer(twice) };

	ASSERT_TRUE(fromOnce.has_value() && fromTwice.has_value());
	EXPECT_EQ(fromTwice->rules.size(), fromOnce->rules.size());
	EXPECT_EQ(fromTwice->players.size(), fromOnce->players.size());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, ServerWithoutItsInfoPacketHasNoReply)
{
	std::vector<std::vector<std::byte>> const datagrams{ LoadServer("173.225.184.201_7778") };

	ASSERT_FALSE(datagrams.empty());
	EXPECT_EQ(ReadServer(std::span{ datagrams }.subspan(1)), std::unexpected{ EParseError::Truncated });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CUnreal2ProtocolTest, CutInfoPacketIsMalformed)
{
	std::vector<std::byte> info{ LoadFixture("ut2004/status-74.91.113.85_7778-00.bin") };

	info.resize(20);

	std::vector<std::vector<std::byte>> const datagrams{ info };

	EXPECT_EQ(ReadServer(datagrams), std::unexpected{ EParseError::Malformed });
}
} // namespace
} // namespace Lkt::Query
