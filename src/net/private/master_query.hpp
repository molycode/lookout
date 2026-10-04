#pragma once

#include "query/game.hpp"
#include "query/server_address.hpp"

namespace Lkt::Net
{
struct SMasterQuery final
{
	Query::EGame game{ Query::NoGame };
	Query::SServerAddress address;
};
} // namespace Lkt::Net
