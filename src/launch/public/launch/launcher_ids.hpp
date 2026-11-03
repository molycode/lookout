#pragma once

#include <string_view>

namespace Lkt::Launch
{
inline constexpr std::string_view InstallDirLauncherId{ "install-dir" };
inline constexpr std::string_view SteamLauncherId{ "steam" };
inline constexpr std::string_view GogLauncherId{ "gog" };
inline constexpr std::string_view StoreLauncherIdPrefix{ "store:" };
} // namespace Lkt::Launch
