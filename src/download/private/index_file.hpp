#pragma once

#include <cstddef>
#include <string>

namespace Lkt::Download
{
struct SIndexFile final
{
	std::string name;
	size_t size{ 0 };
	// Lower-case hex.
	std::string sha256;
};
} // namespace Lkt::Download
