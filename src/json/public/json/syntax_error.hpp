#pragma once

#include <string>
#include <string_view>

namespace Lkt::Json
{
// Where and why the text stops being JSON ("line 3, column 5: syntax error while parsing …"); empty for valid JSON.
std::string DescribeSyntaxError(std::string_view text);
} // namespace Lkt::Json
