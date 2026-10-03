#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace Lkt::Config
{
inline constexpr uint32_t NoPingLimit{ std::numeric_limits<uint32_t>::max() };

struct SServerFilter final
{
	std::string search;
	bool showEmpty{ true };
	bool showFull{ true };
	uint32_t maxPingMs{ NoPingLimit };
	std::string mod;
	std::string country;

	bool operator==(SServerFilter const&) const = default;
};
} // namespace Lkt::Config
