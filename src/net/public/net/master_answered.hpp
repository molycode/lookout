#pragma once

#include "query/game.hpp"
#include <cstddef>
#include <cstdint>

namespace Lkt::Net
{
struct SMasterAnswered final
{
	Query::EGame game{ Query::NoGame };
	size_t index{ 0 };
	uint32_t numServers{ 0 };
	uint32_t refreshId{ 0 };
};
} // namespace Lkt::Net
