#pragma once

#include "query/linux_store_install.hpp"
#include "query/store_libraries.hpp"
#include "query/windows_store_install.hpp"
#include <string>
#include <vector>

namespace Lkt::Query
{
struct SStoreDefinition final
{
	std::string key{};
	std::string name{};
	std::vector<std::string> arguments{};
	EStoreLibraries libraries{ EStoreLibraries::Steam };
	std::vector<SWindowsStoreInstall> onWindows{};
	std::vector<SLinuxStoreInstall> onLinux{};

	bool operator==(SStoreDefinition const&) const = default;
};
} // namespace Lkt::Query
