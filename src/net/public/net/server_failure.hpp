#pragma once

#include <cstdint>

namespace Lkt::Net
{
enum class EServerFailure : uint8_t
{
	NoAnswer,
	BadReply
};
} // namespace Lkt::Net
