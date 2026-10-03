#pragma once

#include "program_error.hpp"
#include <expected>
#include <filesystem>
#include <string_view>

namespace Lkt::Launch
{
// As the exec resolves it, after the working-directory change: a bare name through PATH, a relative path against it.
std::expected<std::filesystem::path, EProgramError> ResolveProgram(std::string_view program, std::filesystem::path const& workingDir, std::string_view searchPath);

std::expected<void, EProgramError> CheckExecutable(std::filesystem::path const& path);
} // namespace Lkt::Launch
