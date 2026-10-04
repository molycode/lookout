#pragma once

#include "callback.hpp"
#include "script/conversation_kind.hpp"
#include "script/script_action.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>

namespace Lkt::Script
{
struct SConversationCall final
{
	EConversationKind kind{ EConversationKind::Master };
	ECallback callback{ ECallback::Start };
	uint64_t id{ 0 };
	int states{ 0 };
	int function{ 0 };
	std::map<std::string, std::string> const* pOptions{ nullptr };
	std::span<std::byte const> data;
	SScriptAction action;
	std::string unknownField;
	std::string problem;
};
} // namespace Lkt::Script
