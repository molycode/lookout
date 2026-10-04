#pragma once

#include <cstdint>

namespace Lkt::Net
{
// Failed waits for the next update to be reported, then becomes Done like an answered master. A resolved master is
// Querying before its first query too.
enum class EMasterState : uint8_t
{
	Resolving,
	Querying,
	Failed,
	Done
};
} // namespace Lkt::Net
