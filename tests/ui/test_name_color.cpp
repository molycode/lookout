#include "name_color.hpp"
#include <gtest/gtest.h>

namespace Lkt::Ui
{
namespace
{
constexpr SRgb ThemeText{ 0.902f, 0.886f, 0.847f };
constexpr float Tolerance{ 0.001f };

//////////////////////////////////////////////////////////////////////////
TEST(NameColor, BlackIsLiftedToTheMinimum)
{
	EXPECT_NEAR(GetLuma(LiftDarkColor(SRgb{ 0.0f, 0.0f, 0.0f }, ThemeText)), MinNameLuma, Tolerance);
}

//////////////////////////////////////////////////////////////////////////
TEST(NameColor, BlueKeepsItsHueWhileLifted)
{
	SRgb const lifted{ LiftDarkColor(SRgb{ 0.0f, 0.0f, 1.0f }, ThemeText) };

	EXPECT_NEAR(GetLuma(lifted), MinNameLuma, Tolerance);
	EXPECT_GT(lifted.b, lifted.r);
	EXPECT_GT(lifted.b, lifted.g);
}

//////////////////////////////////////////////////////////////////////////
TEST(NameColor, BrightColorIsUntouched)
{
	SRgb const green{ 0.0f, 1.0f, 0.0f };
	SRgb const kept{ LiftDarkColor(green, ThemeText) };

	EXPECT_EQ(kept.r, green.r);
	EXPECT_EQ(kept.g, green.g);
	EXPECT_EQ(kept.b, green.b);
}
} // namespace
} // namespace Lkt::Ui
