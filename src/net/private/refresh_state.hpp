#pragma once

#include "refresh_stats.hpp"
#include "net/clock.hpp"
#include <cstdint>
#include <unordered_set>

namespace Lkt::Net
{
struct SRefreshState final
{
	bool isActive{ false };
	uint32_t generation{ 0 };
	Clock::time_point startedAt{};
	std::unordered_set<uint64_t> knownServers;
	SRefreshStats stats;
};
} // namespace Lkt::Net
