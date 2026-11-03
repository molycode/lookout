#pragma once

#include "query/launch_hints.hpp"
#include <string_view>

namespace Lkt::Query
{
inline constexpr std::string_view SteamStore{ "steam" };
inline constexpr std::string_view GogStore{ "gog" };

std::string_view FindStoreId(SLaunchHints const& hints, std::string_view store);
} // namespace Lkt::Query
