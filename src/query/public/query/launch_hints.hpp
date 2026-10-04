#pragma once

#include <span>
#include <string_view>

namespace Lkt::Query
{
// Desktop-file ids in order of preference; installDir is relative to $HOME, and the other paths to installDir.
struct SLaunchHints final
{
	std::span<std::string_view const> desktopFiles;
	std::string_view installDir;
	std::string_view program;
	std::span<std::string_view const> requiredFiles;
};
} // namespace Lkt::Query
