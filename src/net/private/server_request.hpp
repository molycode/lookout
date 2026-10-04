#pragma once

#include "query/game.hpp"
#include "query/server_address.hpp"
#include <cstdint>

namespace Lkt::Net
{
struct SServerRequest final
{
	Query::EGame game{ Query::NoGame };
	Query::SServerAddress address;
	uint32_t numAttempts{ 0 };
};
} // namespace Lkt::Net
