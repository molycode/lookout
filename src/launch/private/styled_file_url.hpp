#pragma once

#include "path_style.hpp"
#include <string>
#include <string_view>

namespace Lkt::Launch
{
std::string ToFileUrl(std::string_view path, EPathStyle style);
} // namespace Lkt::Launch
