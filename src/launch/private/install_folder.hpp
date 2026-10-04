#pragma once

#include "query/launch_hints.hpp"
#include <expected>
#include <filesystem>
#include <string>

namespace Lkt::Launch
{
// The program to run, or a phrase for "'<folder>' cannot start <game>: <reason>".
std::expected<std::filesystem::path, std::string> CheckInstallFolder(Query::SLaunchHints const& hints, std::filesystem::path const& folder);
} // namespace Lkt::Launch
