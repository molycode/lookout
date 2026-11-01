#pragma once

#include <span>
#include <string>
#include <string_view>

namespace Lkt::Cli
{
std::string FormatGameKeys(std::span<std::string_view const> gameKeys);
} // namespace Lkt::Cli
