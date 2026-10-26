#pragma once

#include "query/protocol_definition.hpp"
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Ui
{
struct SFormProtocols final
{
	std::span<Query::SProtocolDefinition const> definitions;
	std::function<bool(std::string& protocol)> drawMoreChoices;
	std::function<void(std::string_view protocol)> drawAbsent;
};
} // namespace Lkt::Ui
