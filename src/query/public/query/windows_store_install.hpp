#pragma once

#include "query/store_registry_value.hpp"
#include <string>

namespace Lkt::Query
{
struct SWindowsStoreInstall final
{
	SStoreRegistryValue folder{};
	std::string program{};

	bool operator==(SWindowsStoreInstall const&) const = default;
};
} // namespace Lkt::Query
