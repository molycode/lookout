#pragma once

#include "index_file.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Lkt::Download
{
struct SIndexGame final
{
	std::string key;
	std::string name;
	uint64_t format{ 0 };
	std::string protocol;
	std::vector<SIndexFile> files;
};
} // namespace Lkt::Download
