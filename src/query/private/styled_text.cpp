#include "query/styled_text.hpp"
#include "query/utf8.hpp"
#include <algorithm>
#include <array>

namespace Lkt::Query
{
namespace
{
// Windows-1252's 0x80-0x9F, where Latin-1 has invisible controls; zero marks the five bytes it leaves undefined.
constexpr std::array<char32_t, 32> Windows1252HighControls
{
	U'\u20AC', 0, U'\u201A', U'\u0192', U'\u201E', U'\u2026', U'\u2020', U'\u2021',
	U'\u02C6', U'\u2030', U'\u0160', U'\u2039', U'\u0152', 0, U'\u017D', 0,
	0, U'\u2018', U'\u2019', U'\u201C', U'\u201D', U'\u2022', U'\u2013', U'\u2014',
	U'\u02DC', U'\u2122', U'\u0161', U'\u203A', U'\u0153', 0, U'\u017E', U'\u0178'
};
constexpr uint8_t NotHex{ 0xFF };

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
bool IsPaletteCode(STextStyle const& style, unsigned char c)
{
	bool const isAlphanumeric{ (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') };
	bool const isPrintable{ c > ' ' && c < 0x7F && c != static_cast<unsigned char>(style.escape) };

	return (style.codes == EColorCodes::Alphanumeric && isAlphanumeric) || (style.codes == EColorCodes::Printable && isPrintable);
}

//////////////////////////////////////////////////////////////////////////
uint8_t GetHexValue(char c)
{
	uint8_t value{ NotHex };

	if (c >= '0' && c <= '9')
	{
		value = static_cast<uint8_t>(c - '0');
	}
	else if (c >= 'a' && c <= 'f')
	{
		value = static_cast<uint8_t>(c - 'a' + 10);
	}
	else if (c >= 'A' && c <= 'F')
	{
		value = static_cast<uint8_t>(c - 'A' + 10);
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
bool IsHexDigit(char c)
{
	return GetHexValue(c) != NotHex;
}

//////////////////////////////////////////////////////////////////////////
// The hex code whose prefix and digits follow the escape at index, or nullptr.
SHexColorCode const* FindHexCode(STextStyle const& style, std::string_view raw, size_t index)
{
	std::string_view const rest{ raw.substr(index + 1) };
	auto const it{ std::ranges::find_if(style.hexCodes, [rest](SHexColorCode const& code)
	{
		return rest.starts_with(code.prefix) && rest.size() - code.prefix.size() >= code.numDigits
			&& std::ranges::all_of(rest.substr(code.prefix.size(), code.numDigits), IsHexDigit);
	}) };

	return (it != style.hexCodes.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
// Three digits are a nibble each, which the games scale to a full byte.
Tge::SColor ReadHexColor(std::string_view digits)
{
	constexpr uint8_t NibbleScale{ 17 };
	std::array<uint8_t, 3> channels{};

	for (size_t channel{ 0 }; channel < channels.size(); ++channel)
	{
		channels[channel] = (digits.size() == channels.size())
			? static_cast<uint8_t>(GetHexValue(digits[channel]) * NibbleScale)
			: static_cast<uint8_t>((GetHexValue(digits[channel * 2]) << 4) | GetHexValue(digits[channel * 2 + 1]));
	}

	return Tge::SColor{ channels[0], channels[1], channels[2] };
}

//////////////////////////////////////////////////////////////////////////
// Zero when no colour code starts at index; a lone escape is text.
size_t GetCodeLength(STextStyle const& style, std::string_view raw, size_t index)
{
	constexpr size_t RgbCodeLength{ 4 };
	constexpr size_t PaletteCodeLength{ 2 };
	bool const isEscape{ style.codes != EColorCodes::None && raw[index] == style.escape };
	SHexColorCode const* const pHexCode{ isEscape ? FindHexCode(style, raw, index) : nullptr };
	size_t length{ 0 };

	if (pHexCode != nullptr)
	{
		length = 1 + pHexCode->prefix.size() + pHexCode->numDigits;
	}
	else if (isEscape && style.codes == EColorCodes::Rgb && raw.size() - index >= RgbCodeLength)
	{
		length = RgbCodeLength;
	}
	else if (isEscape && style.codes != EColorCodes::Rgb && index + 1 < raw.size() && IsPaletteCode(style, static_cast<unsigned char>(raw[index + 1])))
	{
		length = PaletteCodeLength;
	}

	return length;
}

//////////////////////////////////////////////////////////////////////////
Tge::SColor GetCodeColor(STextStyle const& style, std::string_view raw, size_t index)
{
	SHexColorCode const* const pHexCode{ FindHexCode(style, raw, index) };
	Tge::SColor color{};

	if (pHexCode != nullptr)
	{
		color = ReadHexColor(raw.substr(index + 1 + pHexCode->prefix.size(), pHexCode->numDigits));
	}
	else if (style.codes == EColorCodes::Rgb)
	{
		color = Tge::SColor{ static_cast<uint8_t>(raw[index + 1]), static_cast<uint8_t>(raw[index + 2]), static_cast<uint8_t>(raw[index + 3]) };
	}
	else
	{
		int const mask{ static_cast<int>(style.palette.size()) - 1 };

		color = style.palette[static_cast<size_t>((raw[index + 1] - '0') & mask)];
	}

	return color;
}

//////////////////////////////////////////////////////////////////////////
// Every piece between codes must be UTF-8 on its own: a run never splits a character.
bool IsEveryPieceUtf8(STextStyle const& style, std::string_view raw)
{
	bool isUtf8{ true };
	size_t start{ 0 };
	size_t index{ 0 };

	while (index < raw.size())
	{
		size_t const codeLength{ GetCodeLength(style, raw, index) };

		if (codeLength != 0)
		{
			isUtf8 = isUtf8 && IsValidUtf8(raw.substr(start, index - start));
			index += codeLength;
			start = index;
		}
		else
		{
			++index;
		}
	}

	return isUtf8 && IsValidUtf8(raw.substr(start));
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SStyledText DecodeText(STextStyle const& style, std::string_view raw)
{
	bool const isAscii7{ style.encoding == ETextEncoding::Ascii7 };
	// Newer servers send UTF-8; older ones send Windows-1252, which is never valid UTF-8 once it uses accents.
	bool const isUtf8{ !isAscii7 && IsEveryPieceUtf8(style, raw) };
	SStyledText text{};
	STextRun run{};
	size_t index{ 0 };

	while (index < raw.size())
	{
		size_t const codeLength{ GetCodeLength(style, raw, index) };
		unsigned char const c{ static_cast<unsigned char>(raw[index]) };

		if (codeLength != 0)
		{
			FinishRun(run, text);
			run.color = GetCodeColor(style, raw, index);
			run.hasColor = true;
			index += codeLength;
		}
		else
		{
			AppendCharacter(isAscii7 ? static_cast<unsigned char>(c & 0x7Fu) : c, isUtf8, run.text, text.plain);
			++index;
		}
	}

	FinishRun(run, text);

	return text;
}

} // namespace Lkt::Query
