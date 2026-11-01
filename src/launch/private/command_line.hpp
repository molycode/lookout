#pragma once

#include "command_line_error.hpp"
#include "command_syntax.hpp"
#include "quoting.hpp"
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
std::expected<std::vector<std::string>, ECommandLineError> SplitCommandLine(std::string_view text, ECommandSyntax syntax, EQuoting quoting);
std::string QuoteArgument(std::string_view text, ECommandSyntax syntax);

// An argument that was only a file, URL or icon code disappears: the game is started with none of them.
std::expected<std::vector<std::string>, ECommandLineError> ExpandFieldCodes(std::vector<std::string> const& arguments, std::string_view name, std::string_view filePath);
} // namespace Lkt::Launch
