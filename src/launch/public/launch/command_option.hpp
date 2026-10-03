#pragma once

#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include <expected>
#include <string_view>

namespace Lkt::Launch
{
std::expected<SLaunchOption, ELaunchError> MakeCommandOption(std::string_view command);
} // namespace Lkt::Launch
