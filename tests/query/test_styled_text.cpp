#include "query/styled_text.hpp"
#include <gtest/gtest.h>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
bool IsColor(STextRun const& run, uint8_t r, uint8_t g, uint8_t b)
{
	return run.hasColor && run.color.r == r && run.color.g == g && run.color.b == b;
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Quake3CodesSplitTheText)
{
	SStyledText const text{ DecodeText(ETextStyle::Quake3, "^1Red^7White") };

	ASSERT_EQ(text.runs.size(), 2u);
	EXPECT_EQ(text.runs[0].text, "Red");
	EXPECT_TRUE(IsColor(text.runs[0], 255, 0, 0));
	EXPECT_TRUE(IsColor(text.runs[1], 255, 255, 255));
	EXPECT_EQ(text.plain, "RedWhite");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TextBeforeAnyCodeHasNoColor)
{
	SStyledText const text{ DecodeText(ETextStyle::Quake3, "plain") };

	ASSERT_EQ(text.runs.size(), 1u);
	EXPECT_FALSE(text.runs[0].hasColor);
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Quake3WrapsDigitsPastSeven)
{
	SStyledText const text{ DecodeText(ETextStyle::Quake3, "^9x") };

	ASSERT_EQ(text.runs.size(), 1u);
	EXPECT_TRUE(IsColor(text.runs[0], 255, 0, 0));
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Quake3ShowsCaretBeforePunctuation)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "a^!b").plain, "a^!b");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryTakesPunctuationAsCode)
{
	EXPECT_EQ(DecodeText(ETextStyle::EnemyTerritory, "a^!b").plain, "ab");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryUsesItsWidePalette)
{
	SStyledText const text{ DecodeText(ETextStyle::EnemyTerritory, "^Hgreen") };

	ASSERT_EQ(text.runs.size(), 1u);
	EXPECT_TRUE(IsColor(text.runs[0], 0, 102, 51));
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryShowsCaretBeforeSpace)
{
	EXPECT_EQ(DecodeText(ETextStyle::EnemyTerritory, "a^ b").plain, "a^ b");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryCodeNeverSplitsUtf8)
{
	EXPECT_EQ(DecodeText(ETextStyle::EnemyTerritory, "^\xC3\xA9x").plain, "^\xC3\xA9x");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, CaretPairShowsOneCaret)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "^^1x").plain, "^x");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TrailingCaretIsText)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "x^").plain, "x^");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, DropsControlCharacters)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "\x08\x08^1>S").plain, ">S");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, ConvertsWindows1252LettersToUtf8)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "caf\xE9").plain, "caf\xC3\xA9");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, ConvertsWindows1252Quotes)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "\x93hi\x94").plain, "\xE2\x80\x9Chi\xE2\x80\x9D");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, DropsUndefinedWindows1252Bytes)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "a\x81" "b").plain, "ab");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TreatsOverlongUtf8AsWindows1252)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "\xE0\x80\x80").plain, "\xC3\xA0\xE2\x82\xAC\xE2\x82\xAC");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TreatsSurrogateUtf8AsWindows1252)
{
	EXPECT_EQ(DecodeText(ETextStyle::Quake3, "\xED\xA0\x80").plain, "\xC3\xAD\xC2\xA0\xE2\x82\xAC");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, KeepsValidUtf8)
{
	EXPECT_EQ(DecodeText(ETextStyle::EnemyTerritory, "caf\xC3\xA9").plain, "caf\xC3\xA9");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Ascii7MasksTheHighBit)
{
	EXPECT_EQ(DecodeText(ETextStyle::Ascii7, "\xC1\xE2").plain, "Ab");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Ascii7HasNoColorCodes)
{
	EXPECT_EQ(DecodeText(ETextStyle::Ascii7, "^1x").plain, "^1x");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EmptyTextHasNoRuns)
{
	EXPECT_TRUE(DecodeText(ETextStyle::Quake3, "^1").runs.empty());
}
} // namespace
} // namespace Lkt::Query
