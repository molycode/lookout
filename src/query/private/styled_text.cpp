#include "query/styled_text.hpp"
#include "query/utf8.hpp"
#include <array>
#include <span>

namespace Lkt::Query
{
namespace
{
constexpr char ColorEscape{ '^' };

constexpr std::array<Tge::SColor, 8> Quake3Palette
{
	Tge::SColor{ 0, 0, 0 }, Tge::SColor{ 255, 0, 0 }, Tge::SColor{ 0, 255, 0 }, Tge::SColor{ 255, 255, 0 },
	Tge::SColor{ 0, 0, 255 }, Tge::SColor{ 0, 255, 255 }, Tge::SColor{ 255, 0, 255 }, Tge::SColor{ 255, 255, 255 }
};

// ET: Legacy's g_color_table, indexed by (code - '0') & 31.
constexpr std::array<Tge::SColor, 32> EnemyTerritoryPalette
{
	Tge::SColor{ 0, 0, 0 }, Tge::SColor{ 255, 0, 0 }, Tge::SColor{ 0, 255, 0 }, Tge::SColor{ 255, 255, 0 },
	Tge::SColor{ 0, 0, 255 }, Tge::SColor{ 0, 255, 255 }, Tge::SColor{ 255, 0, 255 }, Tge::SColor{ 255, 255, 255 },
	Tge::SColor{ 255, 128, 0 }, Tge::SColor{ 128, 128, 128 }, Tge::SColor{ 191, 191, 191 }, Tge::SColor{ 191, 191, 191 },
	Tge::SColor{ 0, 128, 0 }, Tge::SColor{ 128, 128, 0 }, Tge::SColor{ 0, 0, 128 }, Tge::SColor{ 128, 0, 0 },
	Tge::SColor{ 128, 64, 0 }, Tge::SColor{ 255, 153, 26 }, Tge::SColor{ 0, 128, 128 }, Tge::SColor{ 128, 0, 128 },
	Tge::SColor{ 0, 128, 255 }, Tge::SColor{ 128, 0, 255 }, Tge::SColor{ 51, 153, 204 }, Tge::SColor{ 204, 255, 204 },
	Tge::SColor{ 0, 102, 51 }, Tge::SColor{ 255, 0, 51 }, Tge::SColor{ 179, 26, 26 }, Tge::SColor{ 153, 51, 0 },
	Tge::SColor{ 204, 153, 51 }, Tge::SColor{ 153, 153, 51 }, Tge::SColor{ 255, 255, 191 }, Tge::SColor{ 255, 255, 128 }
};

// Windows-1252's 0x80-0x9F, where Latin-1 has invisible controls; zero marks the five bytes it leaves undefined.
constexpr std::array<char32_t, 32> Windows1252HighControls
{
	U'\u20AC', 0, U'\u201A', U'\u0192', U'\u201E', U'\u2026', U'\u2020', U'\u2021',
	U'\u02C6', U'\u2030', U'\u0160', U'\u2039', U'\u0152', 0, U'\u017D', 0,
	0, U'\u2018', U'\u2019', U'\u201C', U'\u201D', U'\u2022', U'\u2013', U'\u2014',
	U'\u02DC', U'\u2122', U'\u0161', U'\u203A', U'\u0153', 0, U'\u017E', U'\u0178'
};

//////////////////////////////////////////////////////////////////////////
// ioquake3 and iortcw take only letters and digits after the escape; a caret before anything else is shown.
constexpr bool IsQuake3ColorCode(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

//////////////////////////////////////////////////////////////////////////
// ET: Legacy takes any printable ASCII but a caret; bytes above ASCII are excluded so a code never splits UTF-8.
constexpr bool IsEnemyTerritoryColorCode(unsigned char c)
{
	return c > ' ' && c < 0x7F && c != ColorEscape;
}

//////////////////////////////////////////////////////////////////////////
void AppendCodePoint(char32_t codePoint, std::string& run, std::string& plain)
{
	std::array<char, 3> bytes{};
	size_t numBytes{ 1 };

	if (codePoint < 0x80)
	{
		bytes[0] = static_cast<char>(codePoint);
	}
	else if (codePoint < 0x800)
	{
		bytes[0] = static_cast<char>(0xC0 | (codePoint >> 6));
		bytes[1] = static_cast<char>(0x80 | (codePoint & 0x3F));
		numBytes = 2;
	}
	else
	{
		bytes[0] = static_cast<char>(0xE0 | (codePoint >> 12));
		bytes[1] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
		bytes[2] = static_cast<char>(0x80 | (codePoint & 0x3F));
		numBytes = 3;
	}

	run.append(bytes.data(), numBytes);
	plain.append(bytes.data(), numBytes);
}

//////////////////////////////////////////////////////////////////////////
// Control characters are dropped: servers use them for console tricks that only garble a list.
void AppendCharacter(unsigned char c, bool isUtf8, std::string& run, std::string& plain)
{
	if (c >= ' ' && c != 0x7F)
	{
		if (c < 0x80 || isUtf8)
		{
			run += static_cast<char>(c);
			plain += static_cast<char>(c);
		}
		else if (c >= 0xA0)
		{
			AppendCodePoint(static_cast<char32_t>(c), run, plain);
		}
		else if (Windows1252HighControls[static_cast<size_t>(c - 0x80)] != 0)
		{
			AppendCodePoint(Windows1252HighControls[static_cast<size_t>(c - 0x80)], run, plain);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void FinishRun(STextRun& run, SStyledText& text)
{
	if (!run.text.empty())
	{
		text.runs.emplace_back(std::move(run));
	}

	run = STextRun{};
}

//////////////////////////////////////////////////////////////////////////
// Quake 2 engines mark coloured text with the high bit, which only their own font can show.
void DecodeAscii7(std::string_view raw, SStyledText& text)
{
	STextRun run{};

	for (char const c : raw)
	{
		AppendCharacter(static_cast<unsigned char>(static_cast<unsigned char>(c) & 0x7Fu), false, run.text, text.plain);
	}

	FinishRun(run, text);
}

//////////////////////////////////////////////////////////////////////////
template<typename IsColorCode>
void DecodeColorCoded(std::string_view raw, std::span<Tge::SColor const> palette, IsColorCode isColorCode, SStyledText& text)
{
	// Newer servers send UTF-8; older ones send Windows-1252, which is never valid UTF-8 once it uses accents.
	bool const isUtf8{ IsValidUtf8(raw) };
	int const mask{ static_cast<int>(palette.size()) - 1 };
	STextRun run{};
	size_t index{ 0 };

	while (index < raw.size())
	{
		unsigned char const c{ static_cast<unsigned char>(raw[index]) };
		bool const isCode{ c == ColorEscape && index + 1 < raw.size() && isColorCode(static_cast<unsigned char>(raw[index + 1])) };

		if (isCode)
		{
			FinishRun(run, text);
			run.color = palette[static_cast<size_t>((raw[index + 1] - '0') & mask)];
			run.hasColor = true;
			index += 2;
		}
		else
		{
			AppendCharacter(c, isUtf8, run.text, text.plain);
			++index;
		}
	}

	FinishRun(run, text);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SStyledText DecodeText(ETextStyle style, std::string_view raw)
{
	SStyledText text{};

	switch (style)
	{
		case ETextStyle::Ascii7:
			DecodeAscii7(raw, text);
			break;

		case ETextStyle::Quake3:
			DecodeColorCoded(raw, Quake3Palette, IsQuake3ColorCode, text);
			break;

		case ETextStyle::EnemyTerritory:
			DecodeColorCoded(raw, EnemyTerritoryPalette, IsEnemyTerritoryColorCode, text);
			break;
	}

	return text;
}
} // namespace Lkt::Query
