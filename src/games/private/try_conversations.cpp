#include "try_conversations.hpp"
#include "query/game_definition.hpp"
#include "script/protocol_script.hpp"
#include <cstddef>

namespace Lkt::Games
{
//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> TryConversations(Query::SGameDefinition const& game, std::span<std::unique_ptr<Script::CProtocolScript> const> scripts)
{
	Script::CProtocolScript& script{ *scripts[static_cast<size_t>(game.protocol)] };
	std::expected<void, std::string> result{};

	for (Script::EConversationKind const kind : { Script::EConversationKind::Master, Script::EConversationKind::Server })
	{
		Script::SConversation conversation{ kind, 0 };
		std::expected<Script::SScriptAction, std::string> const started{ script.Start(conversation, game.protocolOptions) };

		script.End(conversation);

		if (!started.has_value() && result.has_value())
		{
			result = std::unexpected{ started.error() };
		}
	}

	return result;
}
} // namespace Lkt::Games
