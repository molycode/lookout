#pragma once

#include <cstdint>

namespace Lkt::Net
{
enum class EFetchState : uint8_t
{
	Idle,
	Resolving,
	Connecting,
	Handshaking,
	Sending,
	Receiving
};
} // namespace Lkt::Net
