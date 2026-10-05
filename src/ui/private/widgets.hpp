#pragma once

#include <string_view>

struct ImVec2;
struct ImVec4;

namespace Lkt::Ui
{
// Server text as a label could be cut short at "##"; drawn beside an empty label it never is. The caller pushes an ID.
bool SelectableText(std::string_view text, bool isSelected);
bool IconButton(char const* id, std::string_view glyph);
bool ClearSearchButton();
// Returns where the drawn text ends.
float DrawEllipsised(std::string_view text, ImVec2 const& position, float maxX, ImVec4 const& color);
// "name · location", the location dimmed; the caller pushes an ID.
bool SelectableLauncher(std::string_view name, std::string_view location);
void DrawLauncherLabel(std::string_view name, std::string_view location);
} // namespace Lkt::Ui
