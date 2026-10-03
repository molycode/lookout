#pragma once

#include "master_state.hpp"
#include "net/clock.hpp"
#include "query/game.hpp"
#include "query/server_address.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace Lkt::Net
{
struct SMasterRecord final
{
	Query::EGame game{ Query::EGame::Kingpin };
	uint32_t generation{ 0 };
	std::string_view host;
	uint16_t port{ 0 };
	EMasterState state{ EMasterState::Resolving };
	Query::SServerAddress address;
	Clock::time_point deadline{};
	uint32_t numAttempts{ 0 };
	size_t numEntries{ 0 };
	std::string failure;
};
} // namespace Lkt::Net
