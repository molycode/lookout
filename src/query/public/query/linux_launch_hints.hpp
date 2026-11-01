#pragma once

#include <string>
#include <vector>

namespace Lkt::Query
{
struct SLinuxLaunchHints final
{
	std::vector<std::string> desktopFiles{};
	std::string installDir{};
	std::string program{};
	std::vector<std::string> requiredFiles{};

	bool operator==(SLinuxLaunchHints const&) const = default;
};
} // namespace Lkt::Query
