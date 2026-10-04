#pragma once

#include <cstdint>

namespace Lkt::Script
{
struct SEndCall final
{
	int states{ 0 };
	uint64_t id{ 0 };
};
} // namespace Lkt::Script
