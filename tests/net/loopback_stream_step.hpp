#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

namespace Lkt::Fixtures
{
// The bytes the master waits for, if any, then what it sends, with a pause after its first write.
struct SLoopbackStreamStep final
{
	std::vector<std::byte> expect;
	std::vector<std::byte> send;
	std::chrono::milliseconds pause{ 0 };
};
} // namespace Lkt::Fixtures
