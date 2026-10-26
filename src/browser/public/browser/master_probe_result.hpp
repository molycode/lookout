#pragma once

#include "browser/probe_state.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Browser
{
struct SMasterProbeResult final
{
	EProbeState state{ EProbeState::Probing };
	uint32_t numServers{ 0 };
	std::string failure;
};
} // namespace Lkt::Browser
