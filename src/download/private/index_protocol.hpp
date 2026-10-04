#pragma once

#include "index_file.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Download
{
struct SIndexProtocol final
{
	std::string name;
	int64_t api{ 0 };
	// Named <name>.lua.
	SIndexFile file;
};
} // namespace Lkt::Download
