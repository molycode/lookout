#pragma once

#include <cstdint>
#include <string>

namespace Lkt::Query
{
struct SHexColorCode final
{
	std::string prefix;
	uint8_t numDigits{ 0 };

	bool operator==(SHexColorCode const&) const = default;
};
} // namespace Lkt::Query
