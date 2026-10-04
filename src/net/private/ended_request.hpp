#pragma once

#include "server_request.hpp"
#include "net/clock.hpp"

namespace Lkt::Net
{
// A conversation the clock ended: a step that never got an answer, a quiet period, or the overall deadline.
struct SEndedRequest final
{
	SServerRequest request;
	Clock::duration roundTrip{};
	bool hasAnswered{ false };
};
} // namespace Lkt::Net
