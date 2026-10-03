#pragma once

#include <cstdint>

namespace Lkt::Browser
{
enum class EServerState : uint8_t
{
	Pending,
	Online,
	NoAnswer,
	BadReply
};
} // namespace Lkt::Browser
