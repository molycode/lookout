#include "try_conversations.hpp"
#include "script/protocol_script.hpp"

namespace Lkt::Games
{
//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> TryConversations(Script::CProtocolScript& script, std::map<std::string, std::string> const& options)
{
	std::expected<void, std::string> result{};

	for (Script::EConversationKind const kind : { Script::EConversationKind::Master, Script::EConversationKind::Server })
	{
		Script::SConversation conversation{ kind, 0 };
		std::expected<Script::SScriptAction, std::string> const started{ script.Start(conversation, options) };

		script.End(conversation);

		if (!started.has_value() && result.has_value())
		{
			result = std::unexpected{ started.error() };
		}
	}

	return result;
}
} // namespace Lkt::Games
