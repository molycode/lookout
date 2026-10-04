#pragma once

#include "query/server_address.hpp"
#include <cstddef>

namespace Lkt::Fixtures
{
// This process's open TCP and UDP sockets connected to that address, as the kernel's tables list them, so descriptors
// opened meanwhile by anything else, such as a DNS lookup glibc finishes, cannot disturb the count.
size_t CountSocketsTo(Query::SServerAddress const& peer);
} // namespace Lkt::Fixtures
