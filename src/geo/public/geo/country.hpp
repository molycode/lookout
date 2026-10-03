#pragma once

#include <string_view>

namespace Lkt::Geo
{
struct SCountry final
{
	std::string_view code;
	std::string_view name;
};
} // namespace Lkt::Geo
