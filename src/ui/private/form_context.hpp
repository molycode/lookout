#pragma once

#include "query/protocol_definition.hpp"
#include <cstddef>
#include <functional>
#include <span>

namespace Lkt::Ui
{
struct SFormContext final
{
	std::span<Query::SProtocolDefinition const> definitions;
	std::function<void(size_t row)> drawMasterStatus;
};
} // namespace Lkt::Ui
