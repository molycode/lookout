#pragma once

#include <cstdint>

namespace Lkt::Ui
{
enum class EEditorPending : uint8_t
{
	None,
	Close,
	Open,
	New,
	Prefill,
	Quit
};
} // namespace Lkt::Ui
