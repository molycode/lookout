#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace Lkt::Query
{
std::string_view AsText(std::span<std::byte const> bytes);
std::vector<std::byte> ToBytes(std::string_view text);
} // namespace Lkt::Query
