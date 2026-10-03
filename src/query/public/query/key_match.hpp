#pragma once

#include <string_view>

namespace Lkt::Query
{
struct SKeyMatch final
{
	std::string_view key;
	std::string_view value;
};
} // namespace Lkt::Query
