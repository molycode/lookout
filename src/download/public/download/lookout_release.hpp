#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Lkt::Download
{
inline constexpr std::string_view LookoutReleaseUrl{ "https://github.com/molycode/lookout/releases/latest" };

std::optional<std::array<uint32_t, 3>> ParseLookoutVersion(std::string_view text);
bool IsNewerLookout(std::string_view latest, std::string_view running);
} // namespace Lkt::Download
