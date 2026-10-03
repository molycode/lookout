#pragma once

#include "browser/server_entry.hpp"
#include "config/server_filter.hpp"
#include "config/sort_order.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace Lkt::Browser
{
// Refills rows with indices into entries; a favourite is a row whatever its state or the filter.
void BuildRows(std::span<SServerEntry const> entries, Config::SServerFilter const& filter, Config::SSortOrder const& sort, std::vector<uint32_t>& rows);
} // namespace Lkt::Browser
