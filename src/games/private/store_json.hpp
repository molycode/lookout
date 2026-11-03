#pragma once

#include "query/store_definition.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Games
{
std::expected<Query::SStoreDefinition, std::string> ReadStoreJson(std::string_view text);
} // namespace Lkt::Games
