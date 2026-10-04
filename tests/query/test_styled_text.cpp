#include "fixtures.hpp"
#include "query/styled_text.hpp"
#include <gtest/gtest.h>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
STextStyle const& Quake3Style()
{
	return Fixtures::GetGameByKey("quake3").text;
}

//////////////////////////////////////////////////////////////////////////
STextStyle const& EnemyTerritoryStyle()
{
	return Fixtures::GetGameByKey("et").text;
}

//////////////////////////////////////////////////////////////////////////
STextStyle const& Ascii7Style()
{
	return Fixtures::GetGameByKey("kingpin").text;
}

//////////////////////////////////////////////////////////////////////////
STextStyle MakeRgbStyle()
{
	STextStyle style{};

	style.codes = EColorCodes::Rgb;
	style.escape = '\x1B';

	return style;
}

//////////////////////////////////////////////////////////////////////////
bool IsColor(STextRun const& run, uint8_t r, uint8_t g, uint8_t b)
{
	return run.hasColor && run.color.r == r && run.color.g == g && run.color.b == b;
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Quake3CodesSplitTheText)
{
	SStyledText const text{ DecodeText(Quake3Style(), "^1Red^7White") };

	ASSERT_EQ(text.runs.size(), 2u);
	EXPECT_EQ(text.runs[0].text, "Red");
	EXPECT_TRUE(IsColor(text.runs[0], 255, 0, 0));
	EXPECT_TRUE(IsColor(text.runs[1], 255, 255, 255));
	EXPECT_EQ(text.plain, "RedWhite");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TextBeforeAnyCodeHasNoColor)
{
	SStyledText const text{ DecodeText(Quake3Style(), "plain") };

	ASSERT_EQ(text.runs.size(), 1u);
	EXPECT_FALSE(text.runs[0].hasColor);
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Quake3WrapsDigitsPastSeven)
{
	SStyledText const text{ DecodeText(Quake3Style(), "^9x") };

	ASSERT_EQ(text.runs.size(), 1u);
	EXPECT_TRUE(IsColor(text.runs[0], 255, 0, 0));
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Quake3ShowsCaretBeforePunctuation)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "a^!b").plain, "a^!b");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryTakesPunctuationAsCode)
{
	EXPECT_EQ(DecodeText(EnemyTerritoryStyle(), "a^!b").plain, "ab");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryUsesItsWidePalette)
{
	SStyledText const text{ DecodeText(EnemyTerritoryStyle(), "^Hgreen") };

	ASSERT_EQ(text.runs.size(), 1u);
	EXPECT_TRUE(IsColor(text.runs[0], 0, 102, 51));
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryShowsCaretBeforeSpace)
{
	EXPECT_EQ(DecodeText(EnemyTerritoryStyle(), "a^ b").plain, "a^ b");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EnemyTerritoryCodeNeverSplitsUtf8)
{
	EXPECT_EQ(DecodeText(EnemyTerritoryStyle(), "^\xC3\xA9x").plain, "^\xC3\xA9x");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, CaretPairShowsOneCaret)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "^^1x").plain, "^x");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TrailingCaretIsText)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "x^").plain, "x^");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, DropsControlCharacters)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "\x08\x08^1>S").plain, ">S");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, ConvertsWindows1252LettersToUtf8)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "caf\xE9").plain, "caf\xC3\xA9");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, ConvertsWindows1252Quotes)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "\x93hi\x94").plain, "\xE2\x80\x9Chi\xE2\x80\x9D");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, DropsUndefinedWindows1252Bytes)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "a\x81" "b").plain, "ab");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TreatsOverlongUtf8AsWindows1252)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "\xE0\x80\x80").plain, "\xC3\xA0\xE2\x82\xAC\xE2\x82\xAC");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, TreatsSurrogateUtf8AsWindows1252)
{
	EXPECT_EQ(DecodeText(Quake3Style(), "\xED\xA0\x80").plain, "\xC3\xAD\xC2\xA0\xE2\x82\xAC");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, KeepsValidUtf8)
{
	EXPECT_EQ(DecodeText(EnemyTerritoryStyle(), "caf\xC3\xA9").plain, "caf\xC3\xA9");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Ascii7MasksTheHighBit)
{
	EXPECT_EQ(DecodeText(Ascii7Style(), "\xC1\xE2").plain, "Ab");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, Ascii7HasNoColorCodes)
{
	EXPECT_EQ(DecodeText(Ascii7Style(), "^1x").plain, "^1x");
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, EmptyTextHasNoRuns)
{
	EXPECT_TRUE(DecodeText(Quake3Style(), "^1").runs.empty());
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, RgbCodeCarriesItsColour)
{
	SStyledText const text{ DecodeText(MakeRgbStyle(), "\x1B\xFF\x01\x80Red") };

	ASSERT_EQ(text.runs.size(), 1u);
	EXPECT_EQ(text.runs[0].text, "Red");
	EXPECT_TRUE(IsColor(text.runs[0], 0xFF, 0x01, 0x80));
}

//////////////////////////////////////////////////////////////////////////
TEST(StyledText, CutRgbCodeIsDropped)
{
	EXPECT_EQ(DecodeText(MakeRgbStyle(), "ab\x1B\x01").plain, "ab");
}

//////////////////////////////////////////////////////////////////////////
// The code's bytes are not text, so they do not make UTF-8 text look like Windows-1252.
TEST(StyledText, RgbBytesLeaveUtf8Alone)
{
	EXPECT_EQ(DecodeText(MakeRgbStyle(), "\x1B\xFF\x80\x80" "caf\xC3\xA9").plain, "caf\xC3\xA9");
}

//////////////////////////////////////////////////////////////////////////
// A code inside a UTF-8 character leaves two pieces that are not UTF-8, which then read as Windows-1252.
TEST(StyledText, CodeInsideACharacterMakesEachPieceWindows1252)
{
	EXPECT_EQ(DecodeText(MakeRgbStyle(), "\xC3\x1B\x01\x02\x03\xA9").plain, "\xC3\x83\xC2\xA9");
}
} // namespace
} // namespace Lkt::Query
