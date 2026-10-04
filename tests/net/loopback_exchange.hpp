#pragma once

#include <cstddef>
#include <vector>

namespace Lkt::Fixtures
{
// An empty request matches any; no replies leaves that request unanswered.
struct SLoopbackExchange final
{
	std::vector<std::byte> request;
	std::vector<std::vector<std::byte>> replies;
};
} // namespace Lkt::Fixtures
