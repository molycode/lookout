#pragma once

#include "query/styled_text.hpp"

namespace Lkt::Ui
{
// One line of coloured runs, as a single item for hover checks and tooltips.
void DrawStyledText(Query::SStyledText const& text);
// Clipped by its cell, with the whole text as a tooltip when it does not fit.
void DrawClippedStyledText(Query::SStyledText const& text);
// Wrapped at the available width, as a whole word where a word changes colour.
void DrawWrappedStyledText(Query::SStyledText const& text);
} // namespace Lkt::Ui
