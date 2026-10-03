#pragma once

#include <string>
#include <string_view>

namespace Lkt::Launch
{
// Strict splitting gives the text back as one argument, whatever it holds.
std::string QuoteArgument(std::string_view text);
} // namespace Lkt::Launch
