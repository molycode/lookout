#pragma once

struct ImVec4;

namespace Lkt::Ui
{
struct SThemeColors;

inline constexpr float BaseFontSize{ 16.0f };

// Rebuilds the style from scratch: ScaleAllSizes multiplies, so scaling an already scaled style compounds.
void ApplyTheme(float scale);

ImVec4 GetBackgroundColor();
SThemeColors const& GetThemeColors();
} // namespace Lkt::Ui
