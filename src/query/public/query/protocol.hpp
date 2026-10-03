#pragma once

#include "query/game_definition.hpp"
#include "query/parse_error.hpp"
#include "query/protocol_family.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace Lkt::Query
{
class IProtocol : private Tge::SNoCopyNoMove
{
public:

	virtual ~IProtocol() = default;

	virtual std::vector<std::byte> MasterRequest(SGameDefinition const& game) const = 0;

	virtual std::expected<void, EParseError> ParseMasterReply(std::span<std::byte const> datagram, std::vector<SServerAddress>& servers) const = 0;

	virtual std::vector<std::byte> StatusRequest() const = 0;
	virtual std::expected<SStatusReply, EParseError> ParseStatusReply(std::span<std::byte const> datagram) const = 0;
};

IProtocol const& GetProtocol(EProtocolFamily family);
} // namespace Lkt::Query
