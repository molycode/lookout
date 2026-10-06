#pragma once

#include "index_file.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace Lkt::Download
{
struct SIndexProtocol final
{
	std::string name;
	int64_t api{ 0 };
	std::optional<uint64_t> version{};
	// Named <name>.lua.
	SIndexFile file;
};
} // namespace Lkt::Download
