#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Lkt::Fixtures
{
// An empty request matches any; no replies leaves that request unanswered, and so does each match it ignores first.
struct SLoopbackExchange final
{
	std::vector<std::byte> request;
	std::vector<std::vector<std::byte>> replies;
	uint32_t numIgnored{ 0 };
};
} // namespace Lkt::Fixtures
