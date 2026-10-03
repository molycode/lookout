#pragma once

#include <tge/module/run_context.hpp>
#include <string_view>

namespace Lkt
{
// The directories are borrowed: their strings must outlive the runtime.
Tge::SRunContext MakeRunContext(std::string_view logsDir, std::string_view configDir);
} // namespace Lkt
