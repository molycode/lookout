#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Lkt::Launch
{
std::string ShortenHome(std::string_view path, std::filesystem::path const& home);
} // namespace Lkt::Launch
