#pragma once

#include "query/text_run.hpp"
#include "query/text_style.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Query
{
// UTF-8 throughout: plain is the text without colour, for searching and sorting.
struct SStyledText final
{
	std::string plain;
	std::vector<STextRun> runs;
};

SStyledText DecodeText(STextStyle const& style, std::string_view raw);
} // namespace Lkt::Query
