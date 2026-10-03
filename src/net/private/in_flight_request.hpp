#pragma once

#include "server_request.hpp"
#include "net/clock.hpp"

namespace Lkt::Net
{
struct SInFlightRequest final
{
	SServerRequest request;
	Clock::time_point sentAt{};
};
} // namespace Lkt::Net
