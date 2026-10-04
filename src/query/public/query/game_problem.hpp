#pragma once

#include <string>

namespace Lkt::Query
{
// The key names the game whose description has the problem, so it can be opened to fix it; empty for any other.
struct SGameProblem final
{
	std::string text;
	std::string key;

	bool operator==(SGameProblem const&) const = default;
};
} // namespace Lkt::Query
