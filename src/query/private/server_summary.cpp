#include "query/server_summary.hpp"
#include <algorithm>
#include <charconv>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::string_view FindFirstRule(SStatusReply const& reply, std::span<std::string const> keys)
{
	std::string_view value{};

	for (std::string_view const key : keys)
	{
		if (value.empty())
		{
			value = FindRule(reply, key);
		}
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
std::string_view FindMode(SStatusReply const& reply, std::span<SModeRule const> modes)
{
	auto const it{ std::ranges::find_if(modes, [&reply](SModeRule const& mode) { return FindRule(reply, mode.match.key) == mode.match.value; }) };

	return (it != modes.end()) ? it->label : std::string_view{};
}

//////////////////////////////////////////////////////////////////////////
// Zero when the server does not say, which reads as "unknown" rather than as a full server.
uint32_t ParseCount(std::string_view text)
{
	uint32_t count{ 0 };
	std::from_chars_result const result{ std::from_chars(text.data(), text.data() + text.size(), count) };

	return (result.ec == std::errc{}) ? count : 0;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SServerSummary Summarize(SGameDefinition const& game, SStatusReply const& reply)
{
	SServerSummary summary{};

	summary.name = DecodeText(game.text, FindRule(reply, game.keys.hostname));
	summary.map = DecodeText(game.text, FindRule(reply, game.keys.map)).plain;
	summary.mod = DecodeText(game.text, FindFirstRule(reply, game.keys.mods)).plain;
	summary.mode = FindMode(reply, game.modes);
	summary.numPlayers = static_cast<uint32_t>(reply.players.size());
	summary.maxPlayers = ParseCount(FindRule(reply, game.keys.maxPlayers));
	// Only the lowest bit means a player password: games use the others for things like spectator passwords.
	summary.hasPassword = (ParseCount(FindRule(reply, game.keys.password)) & 1u) != 0;
	summary.isForeign = std::ranges::any_of(game.foreignServers, [&reply](SKeyMatch const& match) { return FindRule(reply, match.key) == match.value; });

	return summary;
}
} // namespace Lkt::Query
