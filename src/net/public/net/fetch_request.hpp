#pragma once

#include <cstddef>
#include <string>

namespace Lkt::Net
{
struct SFetchRequest final
{
	// Absolute on the origin, as "/molycode/lookout-games/main/index.json".
	std::string path;
	// A larger reply fails before its body is read.
	size_t maxSize{ 0 };
};
} // namespace Lkt::Net
