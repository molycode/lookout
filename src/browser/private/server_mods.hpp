#pragma once

#include "browser/server_entry.hpp"
#include <span>
#include <string>
#include <vector>

namespace Lkt::Browser
{
// From every online entry rather than the shown rows, or choosing one mod would hide the others from the choice.
void CollectMods(std::span<SServerEntry const> entries, std::vector<std::string>& mods);
} // namespace Lkt::Browser
