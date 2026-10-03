#pragma once

#include "query/key_match.hpp"
#include <string_view>

namespace Lkt::Query
{
struct SModeRule final
{
	SKeyMatch match;
	std::string_view label;
};
} // namespace Lkt::Query
