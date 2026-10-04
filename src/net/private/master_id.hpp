#pragma once

#include "query/game.hpp"
#include <cstddef>

namespace Lkt::Net
{
// A master by its position in its game's list; a game has one refresh at a time, so this names one record.
struct SMasterId final
{
	Query::EGame game{ Query::NoGame };
	size_t index{ 0 };
};

constexpr bool operator==(SMasterId const& lhs, SMasterId const& rhs)
{
	return lhs.game == rhs.game && lhs.index == rhs.index;
}
} // namespace Lkt::Net
