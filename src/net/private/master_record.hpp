#pragma once

#include "master_state.hpp"
#include "net/clock.hpp"
#include "query/game.hpp"
#include "query/server_address.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Lkt::Net
{
// numAttempts counts the sends of the current step; none yet means the master is resolved and waits to be asked.
struct SMasterRecord final
{
	Query::EGame game{ Query::NoGame };
	size_t index{ 0 };
	uint32_t generation{ 0 };
	std::string_view host;
	uint16_t port{ 0 };
	EMasterState state{ EMasterState::Resolving };
	Query::SServerAddress address;
	Clock::time_point resolveDeadline{};
	Clock::time_point startedAt{};
	Clock::time_point stepSentAt{};
	Clock::time_point lastDatagramAt{};
	std::optional<Clock::duration> quiet;
	uint32_t numAttempts{ 0 };
	uint32_t maxAttempts{ 0 };
	Clock::duration stepTimeout{};
	size_t numEntries{ 0 };
	bool isStepAnswered{ false };
	bool hasAnswered{ false };
	std::string failure;
};
} // namespace Lkt::Net
