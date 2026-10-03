#pragma once

#include "server_request.hpp"
#include "net/clock.hpp"

namespace Lkt::Net
{
struct SAnsweredRequest final
{
	SServerRequest request;
	Clock::duration roundTrip{};
};
} // namespace Lkt::Net
