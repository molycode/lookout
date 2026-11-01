#pragma once

#include <cstddef>
#include <string>

namespace Lkt::Download
{
struct SPendingSwap final
{
	std::string key{};
	std::string name{};
	size_t numTries{ 0 };
};
} // namespace Lkt::Download
