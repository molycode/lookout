#pragma once

#include "launch/found_store.hpp"
#include "launch/launch_environment.hpp"
#include "launch/launch_option.hpp"
#include <span>
#include <vector>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Launch
{
std::vector<SLaunchOption> FindLaunchOptions(Query::SGameDefinition const& game, SLaunchEnvironment const& environment,
	std::span<SFoundStore const> stores);
} // namespace Launch
} // namespace Lkt
