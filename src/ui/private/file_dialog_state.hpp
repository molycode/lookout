#pragma once

#include <cstdint>

namespace Lkt::Ui
{
enum class EFileDialogState : uint8_t
{
	Closed,
	Open,
	Picked,
	Cancelled,
	Failed
};
} // namespace Lkt::Ui
