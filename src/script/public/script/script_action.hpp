#pragma once

#include "query/parse_error.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace Lkt::Script
{
struct SScriptAction final
{
	std::vector<std::vector<std::byte>> send;
	std::vector<Query::SServerAddress> servers;
	std::optional<Query::SStatusReply> reply;
	std::optional<Query::EParseError> reason;
	std::optional<std::chrono::milliseconds> quiet;
	bool isDone{ false };
};
} // namespace Lkt::Script
