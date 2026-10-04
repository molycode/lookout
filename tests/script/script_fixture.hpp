#pragma once

#include "script/protocol_script.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Fixtures
{
// A script that satisfies script API 1 once body has run; body changes `protocol` or probes the sandbox.
std::string MakeScript(std::string_view body);

class CProtocolScriptTest : public testing::Test
{
protected:

	// testing::Test
	void TearDown() override;
	// ~testing::Test

	std::expected<void, std::string> Load(std::string_view body);

	Script::CProtocolScript m_script;
};
} // namespace Lkt::Fixtures
