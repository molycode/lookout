#include "script_fixture.hpp"
#include "conversation_driver.hpp"
#include "fixtures.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Fixtures
{
namespace
{
using Script::EConversationKind;

std::map<std::string, std::string> const NoOptions{};

//////////////////////////////////////////////////////////////////////////
std::string LoadProblem(std::expected<void, std::string> const& loaded)
{
	return loaded.has_value() ? std::string{} : loaded.error();
}

//////////////////////////////////////////////////////////////////////////
std::string Failure(std::expected<Script::SScriptAction, std::string> const& action)
{
	return action.has_value() ? std::string{} : action.error();
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
	EXPECT_EQ(LoadProblem(Load("protocol.server.start = nil")), "server.start must be a function");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MissingSideIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.master = nil")), "master must be a table");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, UnknownFieldIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.parseReply = function() end")), "parseReply is not a field of script API 1");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, UnknownMasterFieldIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.master.finish = function() end")), "master.finish is not a field of script API 1");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, UnknownServerFieldIsRefused)
{
	EXPECT_EQ(LoadProblem(Load("protocol.server.transport = 'udp'")), "server.transport is not a field of script API 1");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MasterTransportMustBeKnown)
{
	EXPECT_EQ(LoadProblem(Load("protocol.master.transport = 'sctp'")), "master.transport must be \"udp\" or \"tcp\"");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MasterTransportIsRead)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.transport = 'tcp' protocol.master.start = function() end")), "");

	EXPECT_EQ(m_script.GetMasterTransport(), Script::EMasterTransport::Tcp);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, TcpMasterSpeaksFirst)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.transport = 'tcp'")), "");

	EXPECT_EQ(Failure(Start(EConversationKind::Master)), "master.start: a TCP master speaks first, so start may not send");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, TcpStartMaySendNothing)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.transport = 'tcp' protocol.master.start = function() end")), "");

	std::expected<Script::SScriptAction, std::string> const started{ Start(EConversationKind::Master) };

	ASSERT_TRUE(started.has_value()) << started.error();
	EXPECT_TRUE(started->send.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, TcpServerStillStartsBySending)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.transport = 'tcp' protocol.server.start = function() end")), "");

	EXPECT_EQ(Failure(Start(EConversationKind::Server)), "server.start: a UDP conversation must start by sending");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FinishMustBeAFunction)
{
	EXPECT_EQ(LoadProblem(Load("protocol.server.finish = 5")), "server.finish must be a function when present");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, SyntaxErrorNamesTheLine)
{
	EXPECT_TRUE(LoadProblem(Load("protocol.api = = 1")).starts_with("test:13:"));
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
TEST_F(CProtocolScriptTest, StartReceivesTheGameOptions)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.start = function(options) return { send = { 'getservers ' .. options.masterQuery } } end")), "");

	std::expected<Script::SScriptAction, std::string> const started{ StartOnce(m_script, EConversationKind::Master, { { "masterQuery", "68 empty full" } }) };

	ASSERT_TRUE(started.has_value()) << started.error();
	EXPECT_EQ(started->send, std::vector<std::vector<std::byte>>{ ToBytes("getservers 68 empty full") });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StartSendsEveryDatagramInOrder)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.start = function() return { send = { 'one', 'two' } } end")), "");

	std::expected<Script::SScriptAction, std::string> const started{ Start(EConversationKind::Server) };

	ASSERT_TRUE(started.has_value()) << started.error();
	EXPECT_EQ(started->send, (std::vector<std::vector<std::byte>>{ ToBytes("one"), ToBytes("two") }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StartThatSendsNothingFails)
{
	for (std::string_view const result : { "nil", "{}" })
	{
		ASSERT_EQ(LoadProblem(Load(std::format("protocol.server.start = function() return {} end", result))), "");

		EXPECT_EQ(Failure(Start(EConversationKind::Server)), "server.start: a UDP conversation must start by sending") << result;
		m_script.Terminate();
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StartMayOnlySend)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.start = function() return { send = { 'x' }, quiet = 100 } end")), "");

	EXPECT_EQ(Failure(Start(EConversationKind::Master)), "master.start: the action may not hold quiet");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ResultThatIsNotATableFails)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.start = function() return 'status' end")), "");

	EXPECT_EQ(Failure(Start(EConversationKind::Server)), "server.start: the result must be an action table or nil");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, SendHoldsOneToEightDatagrams)
{
	for (std::string_view const send : { "{}", "{ 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i' }" })
	{
		ASSERT_EQ(LoadProblem(Load(std::format("protocol.server.start = function() return {{ send = {} }} end", send))), "");

		EXPECT_EQ(Failure(Start(EConversationKind::Server)), "server.start: send must hold 1 to 8 strings of 1 to 65507 bytes") << send;
		m_script.Terminate();
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, DatagramToSendHoldsOneTo65507Bytes)
{
	for (std::string_view const datagram : { "''", "('x'):rep(65508)" })
	{
		ASSERT_EQ(LoadProblem(Load(std::format("protocol.server.start = function() return {{ send = {{ {} }} }} end", datagram))), "");

		EXPECT_EQ(Failure(Start(EConversationKind::Server)), "server.start: send must hold 1 to 8 strings of 1 to 65507 bytes") << datagram;
		m_script.Terminate();
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MasterReceiveListsServers)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { servers = { { ip = 0x2D5E3A3C, port = 27960 } } } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Master, "x") };

	ASSERT_TRUE(action.has_value()) << action.error();
	ASSERT_EQ(action->servers.size(), 1u);
	EXPECT_EQ(action->servers.front(), (Query::SServerAddress{ 0x2D5E3A3C, 27960 }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ServersBeforeAReasonAreKept)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { servers = { { ip = 1, port = 2 } }, reason = 'truncated' } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Master, "x") };

	ASSERT_TRUE(action.has_value()) << action.error();
	EXPECT_EQ(action->servers.size(), 1u);
	EXPECT_EQ(action->reason, Query::EParseError::Truncated);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MasterMayEndItsList)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { servers = {}, done = true } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Master, "x") };

	ASSERT_TRUE(action.has_value()) << action.error();
	EXPECT_TRUE(action->isDone);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, DoneMustBeTrue)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { done = false } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Master, "x")), "master.receive: done must be true");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, QuietIsRead)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { quiet = 1500 } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Master, "x") };

	ASSERT_TRUE(action.has_value()) << action.error();
	EXPECT_EQ(action->quiet, std::chrono::milliseconds{ 1500 });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, QuietOutOfRangeFails)
{
	for (std::string_view const quiet : { "0", "10001", "1.5" })
	{
		ASSERT_EQ(LoadProblem(Load(std::format("protocol.master.receive = function() return {{ quiet = {} }} end", quiet))), "");

		EXPECT_EQ(Failure(Receive(EConversationKind::Master, "x")), "master.receive: quiet must be an integer from 1 to 10000 milliseconds") << quiet;
		m_script.Terminate();
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, SendExcludesQuiet)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { send = { 'next' }, quiet = 100 } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Master, "x")), "master.receive: send and quiet exclude each other");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, DoneExcludesSend)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { send = { 'next' }, done = true } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Master, "x")), "master.receive: an action that ends the conversation excludes send and quiet");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MasterMayNotReply)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { reply = { rules = {}, players = {} } } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Master, "x")), "master.receive: the action may not hold reply");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ServerMayNotListServers)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { servers = {} } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: the action may not hold servers");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ServerMayNotBeDone)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { done = true } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: the action may not hold done");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ActionWithUnnamedEntriesFails)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { 'reply' } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: the action may hold only named fields");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, NilKeepsWaiting)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return nil end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Server, "x") };

	ASSERT_TRUE(action.has_value()) << action.error();
	EXPECT_TRUE(action->send.empty());
	EXPECT_FALSE(action->reply.has_value());
	EXPECT_FALSE(action->reason.has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, PortOutOfRangeFailsTheScript)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { servers = { { ip = 1, port = 2 }, { ip = 1, port = 65536 } } } end")), "");

	EXPECT_TRUE(Failure(Receive(EConversationKind::Master, "x")).starts_with("master.receive: servers[2]"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FractionalNumberIsNotAnInteger)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function() return { servers = { { ip = 1, port = 27960.5 } } } end")), "");

	EXPECT_TRUE(Failure(Receive(EConversationKind::Master, "x")).starts_with("master.receive: servers[1]"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, DatagramKeepsItsNulBytes)
{
	ASSERT_EQ(LoadProblem(Load("protocol.master.receive = function(state, datagram) return { servers = { { ip = 0, port = #datagram } } } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Master, std::string_view{ "a\0b\0c", 5 }) };

	ASSERT_TRUE(action.has_value()) << action.error();
	ASSERT_EQ(action->servers.size(), 1u);
	EXPECT_EQ(action->servers.front().port, 5);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StatusReplyIsRead)
{
	ASSERT_EQ(LoadProblem(Load(R"lua(protocol.server.receive = function()
		return { reply = { rules = { { key = "mapname", value = "q3dm17" } }, players = { { name = "a\0b", score = -3, ping = 48 } },
			malformedPlayerLines = 1 } }
	end)lua")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Server, "x") };

	ASSERT_TRUE(action.has_value()) << action.error();
	ASSERT_TRUE(action->reply.has_value());

	Query::SStatusReply const& reply{ *action->reply };

	ASSERT_EQ(reply.rules.size(), 1u);
	EXPECT_EQ(reply.rules.front().key, "mapname");
	EXPECT_EQ(reply.rules.front().value, "q3dm17");
	ASSERT_EQ(reply.players.size(), 1u);
	EXPECT_EQ(reply.players.front().name, std::string_view("a\0b", 3));
	EXPECT_EQ(reply.players.front().score, -3);
	EXPECT_EQ(reply.players.front().ping, 48u);
	EXPECT_EQ(reply.numMalformedPlayerLines, 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ReasonGivesItsParseError)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { reason = 'wrongHeader' } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Server, "x") };

	ASSERT_TRUE(action.has_value()) << action.error();
	EXPECT_EQ(action->reason, Query::EParseError::WrongHeader);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ReplyExcludesAReason)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { reply = { rules = {}, players = {} }, reason = 'truncated' } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: reply and reason exclude each other");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ServerReasonExcludesSend)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { reason = 'truncated', send = { 'again' } } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: an action that ends the conversation excludes send and quiet");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, UnknownReasonFailsTheScript)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { reason = 'broken' } end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: reason must be \"wrongHeader\", \"truncated\" or \"malformed\"");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, RuntimeErrorNamesTheLine)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() error('no reply here') end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: test:13: no reply here");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ErrorThatIsNotAStringIsDescribed)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() error({}) end")), "");

	EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: the script raised a table as its error");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, PlayerNeedsOnlyAName)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { reply = { rules = {}, players = { { name = 'solo' } } } } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Server, "x") };

	ASSERT_TRUE(action.has_value() && action->reply.has_value()) << action.error_or("");
	ASSERT_EQ(action->reply->players.size(), 1u);
	EXPECT_FALSE(action->reply->players.front().score.has_value());
	EXPECT_FALSE(action->reply->players.front().ping.has_value());
	EXPECT_EQ(action->reply->numMalformedPlayerLines, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ReplyNamesItsJoinPort)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { reply = { rules = {}, players = {}, joinPort = 7777 } } end")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Server, "x") };

	ASSERT_TRUE(action.has_value() && action->reply.has_value()) << action.error_or("");
	EXPECT_EQ(action->reply->joinPort, uint16_t{ 7777 });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ReplyWithoutAJoinPortHasNone)
{
	ASSERT_EQ(LoadProblem(Load("")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Server, "x") };

	ASSERT_TRUE(action.has_value() && action->reply.has_value()) << action.error_or("");
	EXPECT_FALSE(action->reply->joinPort.has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, JoinPortOutOfRangeFailsTheScript)
{
	for (std::string_view const port : { "0", "65536", "'7777'" })
	{
		ASSERT_EQ(LoadProblem(Load(std::format("protocol.server.receive = function() return {{ reply = {{ rules = {{}}, players = {{}}, joinPort = {} }} }} end",
			port))), "");

		EXPECT_EQ(Failure(Receive(EConversationKind::Server, "x")), "server.receive: reply.joinPort must be an integer from 1 to 65535") << port;
		m_script.Terminate();
	}
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, PlayerFieldsKeepTheirOrder)
{
	ASSERT_EQ(LoadProblem(Load(R"lua(protocol.server.receive = function()
		return { reply = { rules = {}, players = { { name = "a", fields = { { key = "team", value = "Blue" }, { key = "time", value = "12" } } } } } }
	end)lua")), "");

	std::expected<Script::SScriptAction, std::string> const action{ Receive(EConversationKind::Server, "x") };

	ASSERT_TRUE(action.has_value() && action->reply.has_value()) << action.error_or("");

	std::vector<Query::SRule> const& fields{ action->reply->players.front().fields };

	ASSERT_EQ(fields.size(), 2u);
	EXPECT_EQ(fields[0].key, "team");
	EXPECT_EQ(fields[0].value, "Blue");
	EXPECT_EQ(fields[1].key, "time");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FieldThatIsNotAPairFailsTheScript)
{
	ASSERT_EQ(LoadProblem(Load("protocol.server.receive = function() return { reply = { rules = {}, players = { { name = 'a', fields = { 'team' } } } } } end")), "");

	EXPECT_TRUE(Failure(Receive(EConversationKind::Server, "x")).starts_with("server.receive: reply.players[1]"));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FinishWithoutAFunctionDoesNothing)
{
	Script::SConversation conversation{ EConversationKind::Server, 0 };

	ASSERT_EQ(LoadProblem(Load("")), "");
	ASSERT_TRUE(m_script.Start(conversation, NoOptions).has_value());

	std::expected<Script::SScriptAction, std::string> const finished{ m_script.Finish(conversation) };

	m_script.End(conversation);

	ASSERT_TRUE(finished.has_value()) << finished.error();
	EXPECT_FALSE(finished->reply.has_value());
	EXPECT_FALSE(finished->reason.has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FinishMayNotSend)
{
	Script::SConversation conversation{ EConversationKind::Server, 0 };

	ASSERT_EQ(LoadProblem(Load("protocol.server.finish = function() return { send = { 'again' } } end")), "");
	ASSERT_TRUE(m_script.Start(conversation, NoOptions).has_value());

	std::expected<Script::SScriptAction, std::string> const finished{ m_script.Finish(conversation) };

	m_script.End(conversation);

	EXPECT_EQ(Failure(finished), "server.finish: the action may not hold send");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StateLastsFromStartToFinish)
{
	Script::SConversation conversation{ EConversationKind::Server, 0 };

	ASSERT_EQ(LoadProblem(Load(R"lua(
		protocol.server.start = function(options, state) state.parts = { "a" } return { send = { "status" } } end
		protocol.server.receive = function(state, datagram) state.parts[#state.parts + 1] = datagram end
		protocol.server.finish = function(state)
			return { reply = { rules = { { key = "parts", value = table.concat(state.parts, ",") } }, players = {} } }
		end)lua")), "");
	ASSERT_TRUE(m_script.Start(conversation, NoOptions).has_value());
	ASSERT_TRUE(m_script.Receive(conversation, ToBytes("b")).has_value());
	ASSERT_TRUE(m_script.Receive(conversation, ToBytes("c")).has_value());

	std::expected<Script::SScriptAction, std::string> const finished{ m_script.Finish(conversation) };

	m_script.End(conversation);

	ASSERT_TRUE(finished.has_value() && finished->reply.has_value()) << finished.error_or("");
	EXPECT_EQ(Query::FindRule(*finished->reply, "parts"), "a,b,c");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ConversationsKeepTheirOwnState)
{
	Script::SConversation first{ EConversationKind::Server, 0 };
	Script::SConversation second{ EConversationKind::Server, 0 };

	ASSERT_EQ(LoadProblem(Load(R"lua(
		protocol.server.receive = function(state, datagram)
			state.seen = (state.seen or "") .. datagram
			return { reply = { rules = { { key = "seen", value = state.seen } }, players = {} } }
		end)lua")), "");
	ASSERT_TRUE(m_script.Start(first, NoOptions).has_value());
	ASSERT_TRUE(m_script.Start(second, NoOptions).has_value());
	ASSERT_TRUE(m_script.Receive(first, ToBytes("a")).has_value());

	std::expected<Script::SScriptAction, std::string> const action{ m_script.Receive(second, ToBytes("b")) };

	m_script.End(first);
	m_script.End(second);

	ASSERT_TRUE(action.has_value() && action->reply.has_value()) << action.error_or("");
	EXPECT_EQ(Query::FindRule(*action->reply, "seen"), "b");
}

//////////////////////////////////////////////////////////////////////////
// A copy ended after the original must not free the state a later conversation was given.
TEST_F(CProtocolScriptTest, EndingTwiceLeavesOtherConversationsAlone)
{
	Script::SConversation first{ EConversationKind::Server, 0 };

	ASSERT_EQ(LoadProblem(Load(R"lua(
		protocol.server.start = function(options, state) state.mark = options.mark return { send = { "status" } } end
		protocol.server.receive = function(state) return { reply = { rules = { { key = "mark", value = state.mark } }, players = {} } } end
		)lua")), "");
	ASSERT_TRUE(m_script.Start(first, { { "mark", "first" } }).has_value());

	Script::SConversation copy{ first };

	m_script.End(first);
	m_script.End(copy);

	Script::SConversation second{ EConversationKind::Server, 0 };
	Script::SConversation third{ EConversationKind::Server, 0 };

	ASSERT_TRUE(m_script.Start(second, { { "mark", "second" } }).has_value());
	ASSERT_TRUE(m_script.Start(third, { { "mark", "third" } }).has_value());

	std::expected<Script::SScriptAction, std::string> const action{ m_script.Receive(second, ToBytes("x")) };

	m_script.End(second);
	m_script.End(third);

	ASSERT_TRUE(action.has_value() && action->reply.has_value()) << action.error_or("");
	EXPECT_EQ(Query::FindRule(*action->reply, "mark"), "second");
}

//////////////////////////////////////////////////////////////////////////
// The states add up to twice the 16 MiB cap, so a state kept after its conversation ends runs the script out of memory.
TEST_F(CProtocolScriptTest, EndedConversationsFreeTheirState)
{
	constexpr size_t NumConversations{ 128 };

	ASSERT_EQ(LoadProblem(Load(R"lua(
		protocol.server.start = function(options, state)
			state.ballast = ("x"):rep(256 << 10)
			if options.ending == "error" then error("failed") end
			if options.ending == "invalid" then return { quiet = 1 } end
			if options.ending == "slow" then while true do end end
			return { send = { "status" } }
		end)lua")), "");

	for (std::string_view const ending : { "none", "error", "invalid", "slow" })
	{
		for (size_t index{ 0 }; index < NumConversations; ++index)
		{
			std::expected<Script::SScriptAction, std::string> const started{ StartOnce(m_script, EConversationKind::Server, { { "ending", std::string{ ending } } }) };

			ASSERT_EQ(started.has_value(), ending == "none") << ending;
			ASSERT_FALSE(Failure(started).contains("not enough memory")) << ending << " " << index;
		}
	}
}
} // namespace
} // namespace Lkt::Fixtures
