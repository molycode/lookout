#pragma once

#include "settings_document.hpp"
#include "settings_json_error.hpp"
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Config
{
inline constexpr uint32_t SettingsVersion{ 2 };

std::string WriteSettingsJson(SSettings const& settings);

// Only a file that is not a JSON object fails as a whole; an invalid value falls back to its default and is counted.
std::expected<SSettingsDocument, ESettingsJsonError> ReadSettingsJson(std::string_view text);
} // namespace Lkt::Config
