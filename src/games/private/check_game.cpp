#include "games/check_game.hpp"
#include "game_json.hpp"
#include "try_conversations.hpp"
#include "query/game_definition.hpp"
#include "script/protocol_script.hpp"
#include <cstddef>
#include <format>
#include <memory>
#include <vector>

namespace Lkt::Games
{
//////////////////////////////////////////////////////////////////////////
std::expected<void, SFieldProblem> CheckGameText(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	std::expected<Query::SGameDefinition, SFieldProblem> const game{ ReadGameFields(text, protocols) };
	std::expected<void, SFieldProblem> result{};

	if (game.has_value())
	{
		Query::SProtocolDefinition const& protocol{ protocols[static_cast<size_t>(game->protocol)] };
		std::vector<std::unique_ptr<Script::CProtocolScript>> scripts(protocols.size());
		std::unique_ptr<Script::CProtocolScript>& pScript{ scripts[static_cast<size_t>(game->protocol)] };

		pScript = std::make_unique<Script::CProtocolScript>();

		std::expected<void, std::string> const loaded{ pScript->Initialize(protocol.name, protocol.source) };

		std::expected<void, std::string> const tried{ loaded.has_value() ? TryConversations(*game, scripts)
			: std::expected<void, std::string>{ std::unexpected{ std::format("the {} protocol cannot be loaded: {}", protocol.name, loaded.error()) } } };

		if (!tried.has_value())
		{
			result = std::unexpected{ SFieldProblem{ {}, tried.error() } };
		}

		pScript->Terminate();
	}
	else
	{
		result = std::unexpected{ game.error() };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Without the conversations, which only the protocol and its options can change, so an editor need not run them on
// every keystroke.
std::expected<void, SFieldProblem> CheckGameFields(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	std::expected<Query::SGameDefinition, SFieldProblem> const game{ ReadGameFields(text, protocols) };
	std::expected<void, SFieldProblem> result{};

	if (!game.has_value())
	{
		result = std::unexpected{ game.error() };
	}

	return result;
}
} // namespace Lkt::Games
