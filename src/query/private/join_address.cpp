#include "query/join_address.hpp"
#include "query/game_definition.hpp"
#include <limits>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::optional<uint16_t> ShiftPort(uint16_t port, int64_t offset)
{
	int64_t const shifted{ static_cast<int64_t>(port) + offset };
	bool const isPort{ shifted >= 1 && shifted <= std::numeric_limits<uint16_t>::max() };

	return isPort ? std::optional<uint16_t>{ static_cast<uint16_t>(shifted) } : std::nullopt;
}
} // namespace

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
