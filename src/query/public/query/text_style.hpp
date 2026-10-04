#pragma once

#include "query/color_codes.hpp"
#include "query/text_encoding.hpp"
#include <tge/color.hpp>
#include <vector>

namespace Lkt::Query
{
// A palette code picks palette[(code - '0') & (palette.size() - 1)]; the palette's size is a power of two.
struct STextStyle final
{
	ETextEncoding encoding{ ETextEncoding::Utf8OrWindows1252 };
	EColorCodes codes{ EColorCodes::None };
	char escape{ '\0' };
	std::vector<Tge::SColor> palette;
};
} // namespace Lkt::Query
