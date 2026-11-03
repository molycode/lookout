#pragma once

#include "launch/found_store.hpp"
#include "launch/launch_environment.hpp"
#include "query/store_definition.hpp"
#include <span>
#include <vector>

namespace Lkt::Launch
{
std::vector<SFoundStore> FindStores(std::span<Query::SStoreDefinition const> stores, SLaunchEnvironment const& environment);
} // namespace Lkt::Launch
