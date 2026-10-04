#pragma once

#include <chrono>
#include <cstdint>

namespace Lkt::Fixtures
{
struct SUt2004ServerSetup final
{
	uint16_t joinPort{ 0 };
	bool hasPlayers{ true };
	bool hasPassword{ false };
	std::chrono::milliseconds packetInterval{ 0 };
};
} // namespace Lkt::Fixtures
