#include "try_conversations.hpp"
#include "query/game_definition.hpp"
#include "script/protocol_script.hpp"
#include <cstddef>
#include <format>
#include <map>
#include <string>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> TryConversation(Script::CProtocolScript& script, Script::EConversationKind kind, std::map<std::string, std::string> const& options)
{
	Script::SConversation conversation{ kind, 0 };
	std::expected<Script::SScriptAction, std::string> const started{ script.Start(conversation, options) };

	script.End(conversation);

	return started.transform([](Script::SScriptAction const&) {});
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<void, SFieldProblem> TryConversations(Query::SGameDefinition const& game, std::span<std::unique_ptr<Script::CProtocolScript> const> scripts)
{
	Script::CProtocolScript& script{ *scripts[static_cast<size_t>(game.protocol)] };
	std::expected<void, SFieldProblem> result{};

	for (Script::EConversationKind const kind : { Script::EConversationKind::Master, Script::EConversationKind::Server })
	{
		std::expected<void, std::string> const tried{ TryConversation(script, kind, game.protocolOptions) };

		if (!tried.has_value() && result.has_value())
		{
			result = std::unexpected{ SFieldProblem{ {}, tried.error() } };
		}
	}

	for (size_t index{ 0 }; index < game.masters.size(); ++index)
	{
		Query::SMasterEndpoint const& master{ game.masters[index] };
		bool const isOwn{ master.protocol != game.protocol || master.protocolOptions != game.protocolOptions };

		if (isOwn && result.has_value())
		{
			std::expected<void, std::string> const tried{ TryConversation(*scripts[static_cast<size_t>(master.protocol)], Script::EConversationKind::Master,
				master.protocolOptions) };

			if (!tried.has_value())
			{
				result = std::unexpected{ SFieldProblem{ std::format("masters[{}]", index), tried.error() } };
			}
		}
	}

	return result;
}
} // namespace Lkt::Games
