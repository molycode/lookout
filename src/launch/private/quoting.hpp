#pragma once

#include <cstdint>

namespace Lkt::Launch
{
// Strict for what the user types, so shell syntax is refused with a reason rather than run as a missing program.
enum class EQuoting : uint8_t
{
	Lenient,
	Strict
};
} // namespace Lkt::Launch
