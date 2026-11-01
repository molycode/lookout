#pragma once

#include "query/linux_launch_hints.hpp"

namespace Lkt::Query
{
struct SLaunchHints final
{
	SLinuxLaunchHints onLinux{};

	bool operator==(SLaunchHints const&) const = default;
};
} // namespace Lkt::Query
