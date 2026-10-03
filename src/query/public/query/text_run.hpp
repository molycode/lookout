#pragma once

#include <tge/color.hpp>
#include <string>

namespace Lkt::Query
{
// Without a colour of its own, a run is drawn in the theme's text colour.
struct STextRun final
{
	std::string text;
	Tge::SColor color{};
	bool hasColor{ false };
};
} // namespace Lkt::Query
