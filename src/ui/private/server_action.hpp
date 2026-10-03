#pragma once

#include "server_action_kind.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Ui
{
// By key, never by entry or row: a game switch, filter or sort applied before it moves those.
struct SServerAction final
{
	EServerAction action{ EServerAction::None };
	uint64_t key{ 0 };
	std::string launcherId{};
};
} // namespace Lkt::Ui
