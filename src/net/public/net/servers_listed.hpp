#pragma once

#include "query/game.hpp"
#include "query/server_address.hpp"
#include <cstdint>
#include <vector>

namespace Lkt::Net
{
// Servers a refresh has learned of and is about to ask; each address arrives once per refresh.
struct SServersListed final
{
	Query::EGame game{ Query::EGame::Kingpin };
	std::vector<Query::SServerAddress> servers;
	uint32_t refreshId{ 0 };
};
} // namespace Lkt::Net
