#pragma once

#include "query/server_address.hpp"
#include <cstddef>

namespace Lkt::Fixtures
{
class CLoopbackServer;

size_t CountSocketsTo(Query::SServerAddress const& peer);
size_t CountSocketsTo(CLoopbackServer const& server);
} // namespace Lkt::Fixtures
