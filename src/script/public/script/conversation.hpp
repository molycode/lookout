#pragma once

#include "script/conversation_kind.hpp"
#include <cstdint>

namespace Lkt::Script
{
// Ids are never reused within a script, so ending a conversation twice cannot touch another one's state.
struct SConversation final
{
	EConversationKind kind{ EConversationKind::Master };
	uint64_t id{ 0 };
};
} // namespace Lkt::Script
