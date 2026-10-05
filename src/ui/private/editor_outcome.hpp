#pragma once

#include <cstdint>

namespace Lkt::Ui
{
enum class EEditorOutcome : uint8_t
{
	None,
	FilesChanged,
	Quit
};
} // namespace Lkt::Ui
