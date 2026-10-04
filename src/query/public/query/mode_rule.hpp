#pragma once

#include "query/key_match.hpp"
#include <string>

namespace Lkt::Query
{
struct SModeRule final
{
	SKeyMatch match;
	std::string label;
};
} // namespace Lkt::Query
