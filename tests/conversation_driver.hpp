#pragma once

#include "query/parse_error.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include "script/conversation_kind.hpp"
#include "script/protocol_script.hpp"
#include "script/script_action.hpp"
#include <cstddef>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace Lkt::Fixtures
{
// One conversation, ended before returning: what its start did.
std::expected<Script::SScriptAction, std::string> StartOnce(Script::CProtocolScript& script, Script::EConversationKind kind,
	std::map<std::string, std::string> const& options);
// One conversation, ended before returning: what it did with one datagram.
std::expected<Script::SScriptAction, std::string> ReceiveOnce(Script::CProtocolScript& script, Script::EConversationKind kind,
	std::map<std::string, std::string> const& options, std::span<std::byte const> data);

// A datagram read the way the fixtures were first asserted: servers plus a reason, or a reply or a reason.
std::expected<void, Query::EParseError> ReadMasterDatagram(Script::CProtocolScript& script, std::map<std::string, std::string> const& options,
	std::span<std::byte const> datagram, std::vector<Query::SServerAddress>& servers);
std::expected<Query::SStatusReply, Query::EParseError> ReadStatusDatagram(Script::CProtocolScript& script, std::span<std::byte const> datagram);

// As the engine reads them: a server's datagrams in order until a reply or reason, then its finish.
std::expected<Query::SStatusReply, Query::EParseError> ReadStatusDatagrams(Script::CProtocolScript& script,
	std::span<std::vector<std::byte> const> datagrams);
// As the engine reads it: a master's stream in pieces of pieceSize, then its end, until the list is done; the error
// says why it is not.
std::expected<void, Query::EParseError> ReadMasterStream(Script::CProtocolScript& script, std::map<std::string, std::string> const& options,
	std::span<std::byte const> stream, size_t pieceSize, std::vector<Query::SServerAddress>& servers);
} // namespace Lkt::Fixtures
