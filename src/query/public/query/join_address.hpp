#pragma once

#include "query/parse_error.hpp"
#include "query/server_address.hpp"
#include <cstdint>
#include <expected>
#include <optional>

namespace Lkt::Query
{
struct SGameDefinition;

// The reply's join port, else the query port less the game's offset, else (outside the port range) the query address.
SServerAddress ToJoinAddress(SGameDefinition const& game, SServerAddress const& queryAddress, std::optional<uint16_t> replyJoinPort);
// Malformed when the game's offset takes the port out of range.
std::expected<SServerAddress, EParseError> ToQueryAddress(SGameDefinition const& game, SServerAddress const& joinAddress);
} // namespace Lkt::Query
