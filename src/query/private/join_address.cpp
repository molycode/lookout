#include "query/join_address.hpp"
#include "query/game_definition.hpp"
#include "query/server_address.hpp"

namespace Lkt::Query
{
//////////////////////////////////////////////////////////////////////////
SServerAddress ToJoinAddress(SGameDefinition const& game, SServerAddress const& queryAddress, std::optional<uint16_t> replyJoinPort)
{
	std::optional<uint16_t> const joinPort{ replyJoinPort.or_else([&game, &queryAddress]
	{
		return ShiftPort(queryAddress.port, -static_cast<int64_t>(game.queryPortOffset));
	}) };

	return SServerAddress{ queryAddress.ipv4, joinPort.value_or(queryAddress.port) };
}

//////////////////////////////////////////////////////////////////////////
std::expected<SServerAddress, EParseError> ToQueryAddress(SGameDefinition const& game, SServerAddress const& joinAddress)
{
	std::optional<uint16_t> const queryPort{ ShiftPort(joinAddress.port, game.queryPortOffset) };
	std::expected<SServerAddress, EParseError> result{ std::unexpected{ EParseError::Malformed } };

	if (queryPort.has_value())
	{
		result = SServerAddress{ joinAddress.ipv4, *queryPort };
	}

	return result;
}
} // namespace Lkt::Query
