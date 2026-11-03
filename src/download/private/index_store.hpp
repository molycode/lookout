#pragma once

#include "index_file.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Download
{
struct SIndexStore final
{
	std::string key{};
	uint64_t format{ 0 };
	SIndexFile file{};
};
} // namespace Lkt::Download
