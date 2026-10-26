#pragma once

#include "query/master_endpoint.hpp"
#include <string>

namespace Lkt::Browser
{
struct SMasterProbeRequest final
{
	std::string key;
	Query::SMasterEndpoint endpoint;
};
} // namespace Lkt::Browser
