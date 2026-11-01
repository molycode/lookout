#pragma once

#include "query/install_dir_base.hpp"
#include <string>

namespace Lkt::Query
{
struct SInstallDirCandidate final
{
	EInstallDirBase base{ EInstallDirBase::ProgramFiles };
	std::string path{};

	bool operator==(SInstallDirCandidate const&) const = default;
};
} // namespace Lkt::Query
