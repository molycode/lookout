#pragma once

#include <tge/io/user_folder.hpp>
#include <expected>
#include <filesystem>
#include <system_error>

namespace Lkt
{
std::expected<std::filesystem::path, std::error_code> GetAppFolder(Tge::IO::EUserFolder folder);
} // namespace Lkt
