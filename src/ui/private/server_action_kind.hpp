#pragma once

#include <cstdint>

namespace Lkt::Ui
{
enum class EServerAction : uint8_t
{
	None,
	Join,
	Refresh,
	ToggleFavourite,
	CopyAddress
};
} // namespace Lkt::Ui
