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

// The settings of games the catalog lacks are carried over from kept, the text last read or written, for their return.
std::string WriteSettingsJson(SSettings const& settings, std::string_view kept = {});

// Only a file that is not a JSON object fails as a whole; an invalid value falls back to its default and is counted.
std::expected<SSettingsDocument, ESettingsJsonError> ReadSettingsJson(std::string_view text);
} // namespace Lkt::Config
