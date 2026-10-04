#pragma once

#include "server_request.hpp"
#include "net/clock.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Lkt::Net
{
// request.numAttempts counts the sends of the current step.
struct SInFlightRequest final
{
	SServerRequest request;
	Clock::time_point startedAt{};
	Clock::time_point stepSentAt{};
	Clock::time_point lastDatagramAt{};
	std::optional<Clock::duration> quiet;
	Clock::duration roundTrip{};
	uint32_t numDatagrams{ 0 };
	size_t numBytes{ 0 };
	bool isStepAnswered{ false };
	bool hasAnswered{ false };
};
} // namespace Lkt::Net
