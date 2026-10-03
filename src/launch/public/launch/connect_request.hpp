#pragma once

#include "query/server_address.hpp"
#include <string>

namespace Lkt::Launch
{
struct SConnectRequest final
{
	Query::SServerAddress address;
	std::string password;
};
} // namespace Lkt::Launch
