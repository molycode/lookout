#pragma once

#include <cstdint>

namespace Lkt::Download
{
enum class EDownloadPhase : uint8_t
{
	Idle,
	ReadingIndex,
	Downloading
};
} // namespace Lkt::Download
