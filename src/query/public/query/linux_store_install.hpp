#pragma once

#include <string>
#include <vector>

namespace Lkt::Query
{
struct SLinuxStoreInstall final
{
	std::string folder{};
	std::vector<std::string> programs{};

	bool operator==(SLinuxStoreInstall const&) const = default;
};
} // namespace Lkt::Query
