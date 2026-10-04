#pragma once

#include "query/parse_error.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Lkt::Script
{
// Exactly one of pServers and pReply is set, by the kind of reply being parsed.
struct SParseCall final
{
	int function{ 0 };
	std::span<std::byte const> datagram;
	std::vector<Query::SServerAddress>* pServers{ nullptr };
	Query::SStatusReply* pReply{ nullptr };
	std::optional<Query::EParseError> reason;
	std::string problem;
};
} // namespace Lkt::Script
