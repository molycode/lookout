#pragma once

#include "query/game.hpp"
#include <netdb.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Lkt::Net
{
// glibc holds pointers into it until the lookup ends, so it lives behind a pointer and never moves.
struct SDnsLookup final
{
	Query::EGame game{ Query::EGame::Kingpin };
	uint32_t generation{ 0 };
	size_t masterIndex{ 0 };
	std::string host;
	addrinfo hints{};
	gaicb request{};
};
} // namespace Lkt::Net
