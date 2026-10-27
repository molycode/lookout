#pragma once

#include "query/master_endpoint.hpp"
#include "query/protocol.hpp"
#include <map>
#include <string>

namespace Lkt::Browser
{
struct SMasterProbeRequest final
{
	std::string key;
	Query::SMasterEndpoint endpoint;
	Query::EProtocol protocol{ Query::NoProtocol };
	std::map<std::string, std::string> protocolOptions;
};
} // namespace Lkt::Browser
