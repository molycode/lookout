#pragma once

#include "query/protocol_definition.hpp"
#include <span>

namespace Lkt::Ui
{
struct SFormProtocols final
{
	std::span<Query::SProtocolDefinition const> definitions;
};
} // namespace Lkt::Ui
