#pragma once

#include "query/linux_launch_hints.hpp"
#include "query/store_id.hpp"
#include "query/windows_launch_hints.hpp"
#include <vector>

namespace Lkt::Query
{
struct SLaunchHints final
{
	SLinuxLaunchHints onLinux{};
	SWindowsLaunchHints onWindows{};
	std::vector<SStoreId> stores{};

	bool operator==(SLaunchHints const&) const = default;
};
} // namespace Lkt::Query
