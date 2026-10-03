#pragma once

#include "query/parse_error.hpp"
#include "query/status_reply.hpp"
#include <expected>
#include <string_view>

namespace Lkt::Query
{
// Both families send the same body after their own header: "\key\value..." on one line, then one line per player.
std::expected<SStatusReply, EParseError> ParseStatusBody(std::string_view body);
} // namespace Lkt::Query
