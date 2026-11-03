#pragma once

#include "query/store_definition.hpp"
#include <span>
#include <string_view>

namespace Lkt::Query
{
void InitializeStoreCatalog(std::span<SStoreDefinition const> stores);
void TerminateStoreCatalog();

std::span<SStoreDefinition const> GetStoreCatalog();
SStoreDefinition const* FindStore(std::string_view key);
} // namespace Lkt::Query
