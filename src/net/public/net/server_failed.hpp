#pragma once

#include "net/server_failure.hpp"
#include "query/game.hpp"
#include "query/server_address.hpp"
#include <cstdint>

namespace Lkt::Net
{
struct SServerFailed final
{
	Query::EGame game{ Query::EGame::Kingpin };
	Query::SServerAddress address;
	EServerFailure failure{ EServerFailure::NoAnswer };
	uint32_t refreshId{ 0 };
};
} // namespace Lkt::Net
