#pragma once

#include "json/json.hpp"

namespace Lkt::Games
{
bool HasPlatformGroups(nlohmann::ordered_json const& game);
bool NeedsPlatformGroups(nlohmann::ordered_json const& game);
void NestLinuxFacts(nlohmann::ordered_json& game);
void FlattenLinuxFacts(nlohmann::ordered_json& game);
} // namespace Lkt::Games
