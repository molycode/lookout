#pragma once

#include "query/protocol.hpp"
#include <cstdint>
#include <map>
#include <string>

namespace Lkt::Query
{
struct SMasterEndpoint final
{
	std::string host;
	uint16_t port{ 0 };
	EProtocol protocol{ NoProtocol };
	std::map<std::string, std::string> protocolOptions;
	int32_t portOffset{ 0 };

	bool operator==(SMasterEndpoint const&) const = default;
};
} // namespace Lkt::Query
