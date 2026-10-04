#pragma once

#include <cstdint>

namespace Lkt::Query
{
// What may follow a game's colour escape.
enum class EColorCodes : uint8_t
{
	None,
	// A letter or digit, indexing the palette.
	Alphanumeric,
	// Any printable ASCII character but the escape, indexing the palette.
	Printable,
	// Three bytes, red, green and blue.
	Rgb
};
} // namespace Lkt::Query
