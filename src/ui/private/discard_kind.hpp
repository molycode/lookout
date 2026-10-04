#pragma once

#include <cstdint>

namespace Lkt::Ui
{
enum class EDiscardKind : uint8_t
{
	// A built-in's changes, which leaves the built-in.
	Changes,
	// A game of the user's own, with its folder.
	Game
};
} // namespace Lkt::Ui
