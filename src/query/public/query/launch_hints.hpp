#pragma once

#include <string>
#include <vector>

namespace Lkt::Query
{
// Desktop-file ids in order of preference; installDir is relative to $HOME, and the other paths to installDir.
struct SLaunchHints final
{
	std::vector<std::string> desktopFiles;
	std::string installDir;
	std::string program;
	std::vector<std::string> requiredFiles;
};
} // namespace Lkt::Query
