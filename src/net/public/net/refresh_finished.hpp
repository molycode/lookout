#pragma once

#include "query/game.hpp"
#include <cstdint>

namespace Lkt::Net
{
struct SRefreshFinished final
{
	Query::EGame game{ Query::NoGame };
	uint32_t refreshId{ 0 };
};
} // namespace Lkt::Net
