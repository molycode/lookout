#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Launch
{
// The program to run, or a phrase for "'<folder>' cannot start <game>: <reason>".
std::expected<std::filesystem::path, std::string> CheckInstallFolder(std::filesystem::path const& folder, std::string_view program,
	std::span<std::string const> requiredFiles);
} // namespace Lkt::Launch
