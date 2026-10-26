#pragma once

#include <cstdint>

namespace Lkt::Net
{
enum class ERefreshKind : uint8_t
{
	Full,
	Probe
};
} // namespace Lkt::Net
