#pragma once

#include "net/loopback_stream_step.hpp"
#include <cstddef>
#include <vector>

namespace Lkt::Fixtures
{
// No steps leaves the master silent until the client goes.
struct SLoopbackStream final
{
	std::vector<SLoopbackStreamStep> steps;
	size_t writeSize{ 1 };
	bool closesWhenDone{ false };
};
} // namespace Lkt::Fixtures
