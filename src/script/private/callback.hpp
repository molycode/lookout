#pragma once

#include <cstdint>

namespace Lkt::Script
{
// In call order; also indexes the script's functions and their names.
enum class ECallback : uint8_t
{
	Start,
	Receive,
	Finish
};
} // namespace Lkt::Script
