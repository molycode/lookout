#pragma once

#include <cstdint>
#include <string>

namespace Lkt::Query
{
// The name as the server sent it; decode it with the game's text style before showing it.
struct SPlayer final
{
	std::string name;
	int32_t score{ 0 };
	uint32_t ping{ 0 };
};
} // namespace Lkt::Query
