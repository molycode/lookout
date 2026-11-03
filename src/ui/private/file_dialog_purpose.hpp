#pragma once

#include <cstdint>

namespace Lkt::Ui
{
enum class EFileDialogPurpose : uint8_t
{
	AddFolder,
	ChangeFolder,
	PickProgram,
	LocateStore
};
} // namespace Lkt::Ui
