#include "conversation_driver.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <utility>

namespace Lkt::Fixtures
{
namespace
{
//////////////////////////////////////////////////////////////////////////
// As the engine takes a server's last action: neither reply nor reason leaves the reply unfinished.
std::expected<Query::SStatusReply, Query::EParseError> ToStatusReply(std::expected<Script::SScriptAction, std::string> action)
{
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

//////////////////////////////////////////////////////////////////////////
bool IsReplyPending(std::expected<Script::SScriptAction, std::string> const& action)
{
	return action.has_value() && !action->reply.has_value() && !action->reason.has_value();
}
} // namespace

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
	return ToStatusReply(ReceiveOnce(script, Script::EConversationKind::Server, {}, datagram));
}

//////////////////////////////////////////////////////////////////////////
std::expected<Query::SStatusReply, Query::EParseError> ReadStatusDatagrams(Script::CProtocolScript& script,
	std::span<std::vector<std::byte> const> datagrams)
{
	Script::SConversation conversation{ Script::EConversationKind::Server, 0 };
	std::expected<Script::SScriptAction, std::string> action{ script.Start(conversation, {}) };

	for (std::vector<std::byte> const& datagram : datagrams)
	{
		if (IsReplyPending(action))
		{
			action = script.Receive(conversation, datagram);
		}
	}

	if (IsReplyPending(action))
	{
		action = script.Finish(conversation);
	}

	script.End(conversation);

	return ToStatusReply(std::move(action));
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, Query::EParseError> ReadMasterStream(Script::CProtocolScript& script, std::map<std::string, std::string> const& options,
	std::span<std::byte const> stream, size_t pieceSize, std::vector<Query::SServerAddress>& servers)
{
	Script::SConversation conversation{ Script::EConversationKind::Master, 0 };
	std::expected<Script::SScriptAction, std::string> action{ script.Start(conversation, options) };
	std::expected<void, Query::EParseError> result{};
	size_t offset{ 0 };
	bool hasEnded{ false };

	while (action.has_value() && !action->isDone && !hasEnded)
	{
		std::span<std::byte const> const piece{ stream.subspan(offset, std::min(pieceSize, stream.size() - offset)) };

		// An empty piece is the end of the stream.
		action = script.Receive(conversation, piece);
		offset += piece.size();
		hasEnded = piece.empty();

		if (action.has_value())
		{
			servers.insert(servers.end(), action->servers.begin(), action->servers.end());
		}
	}

	script.End(conversation);

	EXPECT_TRUE(action.has_value()) << action.error_or("");

	if (!action.has_value() || !action->isDone)
	{
		result = std::unexpected{ action.has_value() ? action->reason.value_or(Query::EParseError::Truncated) : Query::EParseError::ScriptFailed };
	}

	return result;
}
} // namespace Lkt::Fixtures
