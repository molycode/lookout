#pragma once

#include "browser/server_entry.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace Lkt::Browser
{
// From every online entry rather than the shown rows, as with mods; sorted by name.
void CollectCountries(std::span<SServerEntry const> entries, std::vector<uint8_t>& countries);
} // namespace Lkt::Browser
