#pragma once

#include "query/game.hpp"
#include <string>
#include <string_view>

namespace Lkt::Net
{
// An empty failure means the master answered.
struct SMasterOutcome final
{
	Query::EGame game{ Query::NoGame };
	std::string_view host;
	std::string failure;
};
} // namespace Lkt::Net
