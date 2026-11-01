#pragma once

#include "server_request.hpp"
#include "net/clock.hpp"

namespace Lkt::Net
{
// A datagram from a server in flight; the round trip is that of its first datagram.
struct SAnsweredRequest final
{
	SServerRequest request{};
	Clock::duration roundTrip{};
	bool isOverCap{ false };
};
} // namespace Lkt::Net
