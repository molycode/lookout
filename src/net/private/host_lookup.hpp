#pragma once

#include <netdb.h>
#include <string>

namespace Lkt::Net
{
// glibc holds pointers into it until the lookup ends, so it lives behind a pointer and never moves.
struct SHostLookup final
{
	std::string host;
	addrinfo hints{};
	gaicb request{};
};
} // namespace Lkt::Net
