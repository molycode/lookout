#pragma once

#include "script/conversation_kind.hpp"
#include "script/protocol_script.hpp"
#include "script/script_action.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Fixtures
{
// A script that satisfies script API 1 once body has run; body changes `protocol` or probes the sandbox. Its line 13.
std::string MakeScript(std::string_view body);

class CProtocolScriptTest : public testing::Test
{
protected:

	// testing::Test
	void TearDown() override;
	// ~testing::Test

	std::expected<void, std::string> Load(std::string_view body);
	// A whole conversation without options, ended before returning.
	std::expected<Script::SScriptAction, std::string> Start(Script::EConversationKind kind);
	std::expected<Script::SScriptAction, std::string> Receive(Script::EConversationKind kind, std::string_view data);

	Script::CProtocolScript m_script;
};
} // namespace Lkt::Fixtures
