#include "games/check_game.hpp"
#include "game_json.hpp"
#include "try_conversations.hpp"
#include "query/game_definition.hpp"
#include "script/protocol_script.hpp"
#include <cstddef>
#include <format>

namespace Lkt::Games
{
//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> CheckGameText(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	std::expected<Query::SGameDefinition, std::string> const game{ ReadGameJson(text, protocols) };
	std::expected<void, std::string> result{};

	if (game.has_value())
	{
		Query::SProtocolDefinition const& protocol{ protocols[static_cast<size_t>(game->protocol)] };
		Script::CProtocolScript script{};
		std::expected<void, std::string> const loaded{ script.Initialize(protocol.name, protocol.source) };

		if (loaded.has_value())
		{
			result = TryConversations(script, game->protocolOptions);
		}
		else
		{
			result = std::unexpected{ std::format("the {} protocol cannot be loaded: {}", protocol.name, loaded.error()) };
		}

		script.Terminate();
	}
	else
	{
		result = std::unexpected{ game.error() };
	}

	return result;
}
} // namespace Lkt::Games
