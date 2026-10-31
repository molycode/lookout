#pragma once

#include "query/server_address.hpp"
#include <tge/net/ipv4_endpoint.hpp>

namespace Lkt::Net
{
constexpr Tge::Net::SIpv4Endpoint ToEndpoint(Query::SServerAddress const& address)
{
	return Tge::Net::SIpv4Endpoint{ address.ipv4, address.port };
}

constexpr Query::SServerAddress ToServerAddress(Tge::Net::SIpv4Endpoint const& endpoint)
{
	return Query::SServerAddress{ endpoint.address, endpoint.port };
}
} // namespace Lkt::Net
