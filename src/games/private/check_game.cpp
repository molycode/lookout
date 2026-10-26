#include "games/check_game.hpp"
#include "game_json.hpp"
#include "try_conversations.hpp"
#include "query/game_definition.hpp"
#include "query/protocol.hpp"
#include "script/protocol_script.hpp"
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
// A protocol that several fields name is loaded once; a failure is named at the first of them.
std::expected<void, SFieldProblem> LoadScript(std::span<Query::SProtocolDefinition const> protocols, Query::EProtocol protocol, std::string_view path,
	std::vector<std::unique_ptr<Script::CProtocolScript>>& scripts)
{
	std::unique_ptr<Script::CProtocolScript>& pScript{ scripts[static_cast<size_t>(protocol)] };
	std::expected<void, SFieldProblem> result{};

	if (pScript == nullptr)
	{
		Query::SProtocolDefinition const& definition{ protocols[static_cast<size_t>(protocol)] };

		pScript = std::make_unique<Script::CProtocolScript>();

		std::expected<void, std::string> const loaded{ pScript->Initialize(definition.name, definition.source) };

		if (!loaded.has_value())
		{
			result = std::unexpected{ SFieldProblem{ std::string{ path }, std::format("the {} protocol cannot be loaded: {}", definition.name, loaded.error()) } };
		}
	}

	return result;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<void, SFieldProblem> CheckGameText(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	std::expected<Query::SGameDefinition, SFieldProblem> const game{ ReadGameFields(text, protocols) };
	std::expected<void, SFieldProblem> result{};

	if (game.has_value())
	{
		std::vector<std::unique_ptr<Script::CProtocolScript>> scripts(protocols.size());

		result = LoadScript(protocols, game->protocol, {}, scripts);

		for (size_t index{ 0 }; index < game->masters.size() && result.has_value(); ++index)
		{
			result = LoadScript(protocols, game->masters[index].protocol, std::format("masters[{}].protocol", index), scripts);
		}

		if (result.has_value())
		{
			result = TryConversations(*game, scripts);
		}

		for (std::unique_ptr<Script::CProtocolScript> const& pScript : scripts)
		{
			if (pScript != nullptr)
			{
				pScript->Terminate();
			}
		}
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
