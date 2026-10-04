#pragma once

#include "query/game.hpp"
#include "script/conversation.hpp"
#include <cstddef>
#include <vector>

namespace Lkt::Net
{
// What a retry resends is what the conversation started with.
struct SConversationRecord final
{
	Query::EGame game{ Query::NoGame };
	Script::SConversation conversation;
	std::vector<std::vector<std::byte>> send;
};
} // namespace Lkt::Net
