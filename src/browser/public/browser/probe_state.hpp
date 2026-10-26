#pragma once

#include <cstdint>

namespace Lkt::Browser
{
enum class EProbeState : uint8_t
{
	Probing,
	Answered,
	Failed
};
} // namespace Lkt::Browser
