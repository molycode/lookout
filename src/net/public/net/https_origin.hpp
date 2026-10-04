#pragma once

#include "query/server_address.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace Lkt::Net
{
// The one host a fetcher talks to.
struct SHttpsOrigin final
{
	std::string host;
	uint16_t port{ 443 };
	std::string userAgent;
	// For a whole request, the connection it may need included.
	std::chrono::milliseconds timeout{ 30'000 };
	// For tests: a CA file trusted instead of the system's, and an address connected to instead of looking the host up.
	std::string caFile;
	std::optional<Query::SServerAddress> address;
};
} // namespace Lkt::Net
