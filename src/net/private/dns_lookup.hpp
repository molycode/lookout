#pragma once

#include "query/game.hpp"
#include <tge/net/address_lookup.hpp>
#include <cstddef>
#include <cstdint>

namespace Lkt::Net
{
struct SDnsLookup final
{
	Query::EGame game{ Query::NoGame };
	uint32_t generation{ 0 };
	size_t masterIndex{ 0 };
	Tge::Net::CAddressLookup lookup;
};
} // namespace Lkt::Net
