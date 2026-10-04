#pragma once

#include "config/xdg_error.hpp"
#include <expected>
#include <filesystem>

namespace Lkt::Config
{
std::expected<std::filesystem::path, EXdgError> GetConfigHome();
std::expected<std::filesystem::path, EXdgError> GetStateHome();
std::expected<std::filesystem::path, EXdgError> GetDataHome();
} // namespace Lkt::Config
