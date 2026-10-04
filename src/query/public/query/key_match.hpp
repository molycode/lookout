#pragma once

#include <string>

namespace Lkt::Query
{
struct SKeyMatch final
{
	std::string key;
	std::string value;

	bool operator==(SKeyMatch const&) const = default;
};
} // namespace Lkt::Query
