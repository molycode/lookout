#pragma once

#include <string_view>

namespace Lkt::Launch
{
// Any other non-empty id names a desktop file or one of the user's installs; an empty one means the first found.
inline constexpr std::string_view InstallDirLauncherId{ "install-dir" };
} // namespace Lkt::Launch
