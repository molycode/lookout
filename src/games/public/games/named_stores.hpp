#pragma once

#include "json/json.hpp"
#include <string>
#include <vector>

namespace Lkt::Games
{
std::vector<std::string> FindNamedStores(nlohmann::ordered_json const& game);
} // namespace Lkt::Games
