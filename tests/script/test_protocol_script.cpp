#include "script_fixture.hpp"
#include "fixtures.hpp"
#include <gtest/gtest.h>
#include <cstddef>
#include <expected>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Fixtures
{
namespace
{
std::map<std::string, std::string> const NoOptions{};

//////////////////////////////////////////////////////////////////////////
std::string LoadProblem(std::expected<void, std::string> const& loaded)
{
	return loaded.has_value() ? std::string{} : loaded.error();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ScriptMustReturnATable)
{
	EXPECT_EQ(LoadProblem(m_script.Initialize("test", "return 5")), "the script must return a table");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, OtherApiIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.api = 2")), "api must be 1, the script API this Lookout runs");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MissingFunctionIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.statusRequest = nil")), "statusRequest must be a function");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, UnknownFieldIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.parseReply = function() end")), "parseReply is not a field of script API 1");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, SyntaxErrorNamesTheLine)
{
	EXPECT_TRUE(LoadProblem(Load("protocol.api = = 1")).starts_with("test:8:"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, OptionsAreReadInNameOrder)
{
	ASSERT_EQ(LoadProblem(Load(R"lua(protocol.options = {
		zeta = { required = false, description = "Last" },
		alpha = { required = true, description = "First" } })lua")), "");

	ASSERT_EQ(m_script.GetOptions().size(), 2u);
	EXPECT_EQ(m_script.GetOptions()[0].name, "alpha");
	EXPECT_TRUE(m_script.GetOptions()[0].isRequired);
	EXPECT_EQ(m_script.GetOptions()[0].description, "First");
	EXPECT_EQ(m_script.GetOptions()[1].name, "zeta");
	EXPECT_FALSE(m_script.GetOptions()[1].isRequired);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, OptionWithUnknownFieldIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.options = { query = { required = true, description = 'Words', default = 'x' } }")),
		"options.query.default is not a field of an option");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, OptionWithoutDescriptionIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.options = { query = { required = true } }")), "options.query.description must be a non-empty string");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, OptionNeedsAScriptName)
{
	EXPECT_EQ(LoadProblem(Load("protocol.options = { ['master query'] = { required = true, description = 'Words' } }")),
		"options: 'master query' is not a name a script can use");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, RequestReceivesTheGameOptions)
{
	ASSERT_EQ(LoadProblem(Load("protocol.masterRequest = function(options) return 'getservers ' .. options.masterQuery end")), "");

	std::expected<std::vector<std::byte>, std::string> const request{ m_script.MasterRequest({ { "masterQuery", "68 empty full" } }) };

	ASSERT_TRUE(request.has_value()) << request.error();
	EXPECT_EQ(*request, ToBytes("getservers 68 empty full"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, RequestMustBeAString)
{
	ASSERT_EQ(LoadProblem(Load("protocol.statusRequest = function() return 5 end")), "");

	EXPECT_EQ(m_script.StatusRequest(NoOptions).error_or(""), "statusRequest: the request must be a string");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MasterReplyListsServers)
{
	std::vector<Query::SServerAddress> servers{};

	ASSERT_EQ(LoadProblem(Load("protocol.parseMasterReply = function() return { { ip = 0x2D5E3A3C, port = 27960 } } end")), "");

	EXPECT_TRUE(m_script.ParseMasterReply(ToBytes("x"), servers).has_value());
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers.front().ipv4, 0x2D5E3A3Cu);
	EXPECT_EQ(servers.front().port, 27960);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ServersBeforeAReasonAreKept)
{
	std::vector<Query::SServerAddress> servers{};

	ASSERT_EQ(LoadProblem(Load("protocol.parseMasterReply = function() return { { ip = 1, port = 2 } }, 'truncated' end")), "");

	EXPECT_EQ(m_script.ParseMasterReply(ToBytes("x"), servers), std::unexpected{ Query::EParseError::Truncated });
	EXPECT_EQ(servers.size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, PortOutOfRangeFailsTheScript)
{
	std::vector<Query::SServerAddress> servers{};

	ASSERT_EQ(LoadProblem(Load("protocol.parseMasterReply = function() return { { ip = 1, port = 2 }, { ip = 1, port = 65536 } } end")), "");

	EXPECT_EQ(m_script.ParseMasterReply(ToBytes("x"), servers), std::unexpected{ Query::EParseError::ScriptFailed });
	EXPECT_TRUE(servers.empty());
	EXPECT_TRUE(m_script.GetLastFailure().contains("servers[2]")) << m_script.GetLastFailure();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FractionalNumberIsNotAnInteger)
{
	std::vector<Query::SServerAddress> servers{};

	ASSERT_EQ(LoadProblem(Load("protocol.parseMasterReply = function() return { { ip = 1, port = 27960.5 } } end")), "");

	EXPECT_EQ(m_script.ParseMasterReply(ToBytes("x"), servers), std::unexpected{ Query::EParseError::ScriptFailed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, DatagramKeepsItsNulBytes)
{
	std::vector<Query::SServerAddress> servers{};

	ASSERT_EQ(LoadProblem(Load("protocol.parseMasterReply = function(datagram) return { { ip = 0, port = #datagram } } end")), "");

	ASSERT_TRUE(m_script.ParseMasterReply(ToBytes(std::string_view{ "a\0b\0c", 5 }), servers).has_value());
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers.front().port, 5);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StatusReplyIsRead)
{
	ASSERT_EQ(LoadProblem(Load(R"lua(protocol.parseStatusReply = function()
		return { rules = { { key = "mapname", value = "q3dm17" } }, players = { { name = "a\0b", score = -3, ping = 48 } },
			malformedPlayerLines = 1 }
	end)lua")), "");

	std::expected<Query::SStatusReply, Query::EParseError> const reply{ m_script.ParseStatusReply(ToBytes("x")) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->rules.size(), 1u);
	EXPECT_EQ(reply->rules.front().key, "mapname");
	EXPECT_EQ(reply->rules.front().value, "q3dm17");
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_EQ(reply->players.front().name, std::string_view("a\0b", 3));
	EXPECT_EQ(reply->players.front().score, -3);
	EXPECT_EQ(reply->players.front().ping, 48u);
	EXPECT_EQ(reply->numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ReasonGivesItsParseError)
{
	ASSERT_EQ(LoadProblem(Load("protocol.parseStatusReply = function() return nil, 'wrongHeader' end")), "");

	EXPECT_EQ(m_script.ParseStatusReply(ToBytes("x")), std::unexpected{ Query::EParseError::WrongHeader });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ReplyWithAReasonFailsTheScript)
{
	ASSERT_EQ(LoadProblem(Load("protocol.parseStatusReply = function() return { rules = {}, players = {}, malformedPlayerLines = 0 }, 'truncated' end")), "");

	EXPECT_EQ(m_script.ParseStatusReply(ToBytes("x")), std::unexpected{ Query::EParseError::ScriptFailed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, UnknownReasonFailsTheScript)
{
	ASSERT_EQ(LoadProblem(Load("protocol.parseStatusReply = function() return nil, 'broken' end")), "");

	EXPECT_EQ(m_script.ParseStatusReply(ToBytes("x")), std::unexpected{ Query::EParseError::ScriptFailed });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, RuntimeErrorNamesTheLine)
{
	ASSERT_EQ(LoadProblem(Load("protocol.parseStatusReply = function() error('no reply here') end")), "");

	EXPECT_EQ(m_script.ParseStatusReply(ToBytes("x")), std::unexpected{ Query::EParseError::ScriptFailed });
	EXPECT_EQ(m_script.GetLastFailure(), "parseStatusReply: test:8: no reply here");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ErrorThatIsNotAStringIsDescribed)
{
	ASSERT_EQ(LoadProblem(Load("protocol.parseStatusReply = function() error({}) end")), "");

	EXPECT_EQ(m_script.ParseStatusReply(ToBytes("x")), std::unexpected{ Query::EParseError::ScriptFailed });
	EXPECT_EQ(m_script.GetLastFailure(), "parseStatusReply: the script raised a table as its error");
}
//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, PlayerNeedsOnlyAName)
{
	ASSERT_EQ(LoadProblem(Load("protocol.parseStatusReply = function() return { rules = {}, players = { { name = 'solo' } } } end")), "");

	std::expected<Query::SStatusReply, Query::EParseError> const reply{ m_script.ParseStatusReply(ToBytes("x")) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.size(), 1u);
	EXPECT_FALSE(reply->players.front().score.has_value());
	EXPECT_FALSE(reply->players.front().ping.has_value());
	EXPECT_EQ(reply->numMalformedPlayerLines, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, PlayerFieldsKeepTheirOrder)
{
	ASSERT_EQ(LoadProblem(Load(R"lua(protocol.parseStatusReply = function()
		return { rules = {}, players = { { name = "a", fields = { { key = "team", value = "Blue" }, { key = "time", value = "12" } } } } }
	end)lua")), "");

	std::expected<Query::SStatusReply, Query::EParseError> const reply{ m_script.ParseStatusReply(ToBytes("x")) };

	ASSERT_TRUE(reply.has_value());
	ASSERT_EQ(reply->players.front().fields.size(), 2u);
	EXPECT_EQ(reply->players.front().fields[0].key, "team");
	EXPECT_EQ(reply->players.front().fields[0].value, "Blue");
	EXPECT_EQ(reply->players.front().fields[1].key, "time");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FieldThatIsNotAPairFailsTheScript)
{
	ASSERT_EQ(LoadProblem(Load("protocol.parseStatusReply = function() return { rules = {}, players = { { name = 'a', fields = { 'team' } } } } end")), "");

	EXPECT_EQ(m_script.ParseStatusReply(ToBytes("x")), std::unexpected{ Query::EParseError::ScriptFailed });
}

} // namespace
} // namespace Lkt::Fixtures
