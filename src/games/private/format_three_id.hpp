#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Games
{
struct SFormatThreeId final
{
	std::string_view store{};
	std::string_view field{};
	uint64_t max{ 0 };
	std::string_view range{};
};
} // namespace Lkt::Games
