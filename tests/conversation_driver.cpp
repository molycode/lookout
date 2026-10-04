#include "conversation_driver.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
std::expected<Script::SScriptAction, std::string> StartOnce(Script::CProtocolScript& script, Script::EConversationKind kind,
	std::map<std::string, std::string> const& options)
{
	Script::SConversation conversation{ kind, 0 };
	std::expected<Script::SScriptAction, std::string> started{ script.Start(conversation, options) };

	script.End(conversation);

	return started;
}

//////////////////////////////////////////////////////////////////////////
std::expected<Script::SScriptAction, std::string> ReceiveOnce(Script::CProtocolScript& script, Script::EConversationKind kind,
	std::map<std::string, std::string> const& options, std::span<std::byte const> data)
{
	Script::SConversation conversation{ kind, 0 };
	std::expected<Script::SScriptAction, std::string> action{ script.Start(conversation, options) };

	if (action.has_value())
	{
		action = script.Receive(conversation, data);
	}

	script.End(conversation);

	return action;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, Query::EParseError> ReadMasterDatagram(Script::CProtocolScript& script, std::map<std::string, std::string> const& options,
	std::span<std::byte const> datagram, std::vector<Query::SServerAddress>& servers)
{
	std::expected<Script::SScriptAction, std::string> const action{ ReceiveOnce(script, Script::EConversationKind::Master, options, datagram) };
	std::expected<void, Query::EParseError> result{};

	EXPECT_TRUE(action.has_value()) << action.error_or("");

	if (action.has_value())
	{
		servers.insert(servers.end(), action->servers.begin(), action->servers.end());
	}

	if (!action.has_value() || action->reason.has_value())
	{
		result = std::unexpected{ action.has_value() ? *action->reason : Query::EParseError::ScriptFailed };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<Query::SStatusReply, Query::EParseError> ReadStatusDatagram(Script::CProtocolScript& script, std::span<std::byte const> datagram)
{
	std::expected<Script::SScriptAction, std::string> action{ ReceiveOnce(script, Script::EConversationKind::Server, {}, datagram) };
	std::expected<Query::SStatusReply, Query::EParseError> result{ std::unexpected{ Query::EParseError::ScriptFailed } };

	EXPECT_TRUE(action.has_value()) << action.error_or("");

	if (action.has_value() && action->reply.has_value())
	{
		result = std::move(*action->reply);
	}
	else if (action.has_value())
	{
		result = std::unexpected{ action->reason.value_or(Query::EParseError::Truncated) };
	}

	return result;
}
} // namespace Lkt::Fixtures
