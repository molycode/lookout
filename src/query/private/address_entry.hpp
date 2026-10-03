#pragma once

#include "query/server_address.hpp"
#include <string_view>

namespace Lkt::Query
{
inline constexpr size_t AddressEntrySize{ 6 };

// Four address bytes then the port, both in network byte order.
SServerAddress ReadAddressEntry(std::string_view entry);
} // namespace Lkt::Query
