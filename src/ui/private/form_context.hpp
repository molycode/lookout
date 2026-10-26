#pragma once

#include "query/protocol_definition.hpp"
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Ui
{
struct SFormContext final
{
	std::span<Query::SProtocolDefinition const> definitions;
	std::function<bool(std::string& protocol)> drawMoreChoices;
	std::function<void(std::string_view protocol)> drawAbsent;
	std::function<void(size_t row)> drawMasterStatus;
};
} // namespace Lkt::Ui
