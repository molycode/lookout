#pragma once

#include "json/json.hpp"
#include <cstdint>

namespace Lkt::Games
{
bool HasPlatformGroups(nlohmann::ordered_json const& game);
bool NeedsPlatformGroups(nlohmann::ordered_json const& game);
void NestLinuxFacts(nlohmann::ordered_json& game);
void FlattenLinuxFacts(nlohmann::ordered_json& game);
bool HasStoreIds(nlohmann::ordered_json const& game);
void LiftStoreIds(nlohmann::ordered_json& game);
uint64_t FindLowestFormat(nlohmann::ordered_json const& game);
void ShapeForFormat(nlohmann::ordered_json& game, uint64_t format);
} // namespace Lkt::Games
