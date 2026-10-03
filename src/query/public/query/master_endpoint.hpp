#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Query
{
struct SMasterEndpoint final
{
	std::string_view host;
	uint16_t port{ 0 };
};
} // namespace Lkt::Query
