#pragma once

#include "query/key_match.hpp"
#include <string>

namespace Lkt::Query
{
struct SModeRule final
{
	SKeyMatch match;
	std::string label;

	bool operator==(SModeRule const&) const = default;
};
} // namespace Lkt::Query
