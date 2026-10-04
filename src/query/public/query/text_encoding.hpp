#pragma once

#include <cstdint>

namespace Lkt::Query
{
enum class ETextEncoding : uint8_t
{
	// The high bit marks glyphs only the game's own font has, so it is dropped.
	Ascii7,
	// UTF-8 where the text is valid UTF-8, Windows-1252 otherwise.
	Utf8OrWindows1252
};
} // namespace Lkt::Query
