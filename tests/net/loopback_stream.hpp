#pragma once

#include <cstddef>
#include <vector>

namespace Lkt::Fixtures
{
// No greeting leaves the master silent; it then waits for a request that never comes.
struct SLoopbackStream final
{
	std::vector<std::byte> greeting;
	std::vector<std::byte> request;
	std::vector<std::byte> reply;
	size_t writeSize{ 1 };
	bool closesAfterReply{ true };
};
} // namespace Lkt::Fixtures
