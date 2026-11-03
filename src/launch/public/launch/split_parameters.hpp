#pragma once

#include "launch/launch_error.hpp"
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
std::expected<std::vector<std::string>, ELaunchError> SplitParameters(std::string_view parameters);
} // namespace Lkt::Launch
