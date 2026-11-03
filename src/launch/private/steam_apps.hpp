#pragma once

#include "steam_app.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace Lkt::Launch
{
std::optional<SSteamApp> FindInstalledSteamApp(std::span<std::filesystem::path const> libraries, std::string_view appId);
} // namespace Lkt::Launch
