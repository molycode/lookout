#pragma once

#include <cstdint>

namespace Lkt::Games
{
// Where a game's description comes from, as the user folder holds it now.
enum class EGameSource : uint8_t
{
	None,
	Downloaded,
	// A downloaded game with the user's changes to it.
	Patched,
	User
};
} // namespace Lkt::Games
