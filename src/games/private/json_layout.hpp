#pragma once

#include "json/json.hpp"
#include <string>

namespace Lkt::Games
{
std::string WriteJsonLayout(nlohmann::ordered_json const& value);
} // namespace Lkt::Games
