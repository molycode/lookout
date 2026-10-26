#pragma once

#include "query/game.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace Lkt::Net
{
struct SMasterFailed final
{
	Query::EGame game{ Query::NoGame };
	size_t index{ 0 };
	std::string host;
	std::string reason;
	uint32_t refreshId{ 0 };
};
} // namespace Lkt::Net
