#pragma once

#include <string>

namespace Lkt::Query
{
// An option a protocol script declares; a game sets it under its protocolOptions.
struct SProtocolOption final
{
	std::string name;
	std::string description;
	bool isRequired{ false };

	bool operator==(SProtocolOption const&) const = default;
};
} // namespace Lkt::Query
