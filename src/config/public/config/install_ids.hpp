#pragma once

#include "config/game_install.hpp"
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace Lkt::Config
{
inline constexpr uint32_t MaxInstallId{ std::numeric_limits<uint32_t>::max() };

std::string ToLauncherId(uint32_t installId);
uint32_t NextInstallId(std::span<SGameInstall const> installs);
SGameInstall const* FindInstall(std::span<SGameInstall const> installs, uint32_t id);
} // namespace Lkt::Config
