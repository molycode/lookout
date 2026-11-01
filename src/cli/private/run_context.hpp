#pragma once

#include <tge/module/run_context.hpp>
#include <string_view>

namespace Lkt::Cli
{
Tge::SRunContext MakeRunContext(std::string_view logsDir, std::string_view configDir);
} // namespace Lkt::Cli
