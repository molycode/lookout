#pragma once

#include "query/install_dir_candidate.hpp"
#include <string>
#include <vector>

namespace Lkt::Query
{
struct SWindowsLaunchHints final
{
	std::vector<SInstallDirCandidate> installDirs{};
	std::string program{};
	std::vector<std::string> requiredFiles{};

	bool operator==(SWindowsLaunchHints const&) const = default;
};
} // namespace Lkt::Query
