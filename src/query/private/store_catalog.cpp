#include "query/store_catalog.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <vector>

namespace Lkt::Query
{
namespace
{
std::vector<SStoreDefinition> gStores{};
} // namespace

//////////////////////////////////////////////////////////////////////////
// Apart from the game catalog: the query engine never reads stores, so they can be replaced while it runs.
void InitializeStoreCatalog(std::span<SStoreDefinition const> stores)
{
	TGE_ASSERT(gStores.empty(), "The store catalog is already initialized");

	TGE_ASSERT(std::ranges::all_of(stores, [&stores](SStoreDefinition const& store) { return std::ranges::count(stores, store.key, &SStoreDefinition::key) == 1; }),
		"Two stores share a key");

	gStores.assign(stores.begin(), stores.end());
}

//////////////////////////////////////////////////////////////////////////
void TerminateStoreCatalog()
{
	gStores.clear();
	gStores.shrink_to_fit();
}

//////////////////////////////////////////////////////////////////////////
std::span<SStoreDefinition const> GetStoreCatalog()
{
	return gStores;
}

//////////////////////////////////////////////////////////////////////////
SStoreDefinition const* FindStore(std::string_view key)
{
	auto const it{ std::ranges::find(gStores, key, &SStoreDefinition::key) };

	return (it != gStores.end()) ? &*it : nullptr;
}
} // namespace Lkt::Query
