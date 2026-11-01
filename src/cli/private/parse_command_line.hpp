#pragma once

#include "run_request.hpp"
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Cli
{
std::expected<SRunRequest, std::string> ParseCommandLine(std::span<std::string_view const> options, std::span<std::string_view const> gameKeys);
} // namespace Lkt::Cli
