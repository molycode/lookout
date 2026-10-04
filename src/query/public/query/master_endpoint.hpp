#pragma once

#include <cstdint>
#include <string>

namespace Lkt::Query
{
struct SMasterEndpoint final
{
	std::string host;
	uint16_t port{ 0 };
};
} // namespace Lkt::Query
