#pragma once

#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include <span>

namespace Lkt::Net
{
struct SQueryCatalog final
{
	std::span<Query::SGameDefinition const> games;
	std::span<Query::SProtocolDefinition const> protocols;
};
} // namespace Lkt::Net
