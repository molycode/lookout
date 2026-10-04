#pragma once

#include <cstdint>

namespace Lkt::Download
{
enum class EOfferState : uint8_t
{
	NotInstalled,
	Installed,
	UpdateAvailable,
	// Its game.json format or its protocol's script API is newer than this Lookout reads.
	NeedsNewerLookout,
	// Downloaded once, and no longer in lookout-games.
	Withdrawn
};
} // namespace Lkt::Download
