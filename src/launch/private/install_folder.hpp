#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Launch
{
struct SLaunchOption;

// The program to run, or a phrase for "'<folder>' cannot start <game>: <reason>".
std::expected<std::filesystem::path, std::string> CheckInstallFolder(std::filesystem::path const& folder, std::string_view program,
	std::span<std::string const> requiredFiles);
std::optional<std::filesystem::path> FindInstallProgram(std::filesystem::path const& folder, std::string_view program,
	std::span<std::string const> requiredFiles, std::string_view gameName);
SLaunchOption MakeInstallOption(std::string id, std::string name, std::string location, std::filesystem::path const& program);
} // namespace Lkt::Launch
