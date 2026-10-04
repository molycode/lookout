#pragma once

#include "query/game.hpp"

namespace Lkt::Ui
{
struct SGameMove final
{
	Query::EGame game{ Query::NoGame };
	Query::EGame target{ Query::NoGame };
};
} // namespace Lkt::Ui
