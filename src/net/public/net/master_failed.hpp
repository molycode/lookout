#pragma once

#include "query/game.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Net
{
struct SMasterFailed final
{
	Query::EGame game{ Query::EGame::Kingpin };
	std::string host;
	std::string reason;
	uint32_t refreshId{ 0 };
};
} // namespace Lkt::Net
