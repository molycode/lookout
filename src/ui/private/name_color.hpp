#pragma once

#include "rgb.hpp"

namespace Lkt::Ui
{
// Game colour codes include black and dark blue, which vanish on a dark theme.
inline constexpr float MinNameLuma{ 0.5f };

constexpr float GetLuma(SRgb const& color)
{
	return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

// Mixes a too-dark colour toward the text colour just far enough: luma is linear in the mix, so it lands on the minimum.
constexpr SRgb LiftDarkColor(SRgb const& color, SRgb const& text)
{
	float const luma{ GetLuma(color) };
	SRgb lifted{ color };

	if (luma < MinNameLuma)
	{
		float const t{ (MinNameLuma - luma) / (GetLuma(text) - luma) };

		lifted = SRgb{ color.r + (text.r - color.r) * t, color.g + (text.g - color.g) * t, color.b + (text.b - color.b) * t };
	}

	return lifted;
}
} // namespace Lkt::Ui
