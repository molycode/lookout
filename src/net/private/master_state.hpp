#pragma once

#include <cstdint>

namespace Lkt::Net
{
// Failed waits for the next update to be reported, then becomes Done like an answered master.
enum class EMasterState : uint8_t
{
	Resolving,
	Querying,
	Receiving,
	Failed,
	Done
};
} // namespace Lkt::Net
