#pragma once

#include "launch/found_store.hpp"
#include "launch/launch_environment.hpp"
#include "launch/located_program.hpp"
#include "query/game_definition.hpp"
#include "query/store_definition.hpp"
#include <span>
#include <string>
#include <vector>

namespace Lkt::Launch
{
std::vector<SFoundStore> FindStores(std::span<Query::SStoreDefinition const> stores, SLaunchEnvironment const& environment,
	std::span<SLocatedProgram const> located);
std::vector<std::string> FindStoresWithoutTheirProgram(Query::SGameDefinition const& game, std::span<SFoundStore const> stores);
} // namespace Lkt::Launch
