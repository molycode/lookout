#pragma once

#include <cstdint>

namespace Lkt::Ui
{
enum class EDiscardKind : uint8_t
{
	// The changes to a downloaded game, which leaves it as downloaded.
	Changes,
	// A game of the user's own, with its folder.
	Game
};
} // namespace Lkt::Ui
