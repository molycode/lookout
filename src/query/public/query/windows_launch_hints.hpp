#pragma once

#include "query/install_dir_candidate.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Lkt::Query
{
struct SWindowsLaunchHints final
{
	uint32_t steamAppId{ 0 };
	uint64_t gogId{ 0 };
	std::vector<SInstallDirCandidate> installDirs{};
	std::string program{};
	std::vector<std::string> requiredFiles{};

	bool operator==(SWindowsLaunchHints const&) const = default;
};
} // namespace Lkt::Query
