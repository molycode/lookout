#pragma once

#include "query/store_registry_root.hpp"
#include <string>

namespace Lkt::Query
{
struct SStoreRegistryValue final
{
	EStoreRegistryRoot root{ EStoreRegistryRoot::CurrentUser };
	std::string key{};
	std::string value{};

	bool operator==(SStoreRegistryValue const&) const = default;
};
} // namespace Lkt::Query
