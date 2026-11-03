#include "query/store_ids.hpp"
#include <algorithm>

namespace Lkt::Query
{
//////////////////////////////////////////////////////////////////////////
std::string_view FindStoreId(SLaunchHints const& hints, std::string_view store)
{
	auto const it{ std::ranges::find(hints.stores, store, &SStoreId::store) };

	return (it != hints.stores.end()) ? std::string_view{ it->id } : std::string_view{};
}
} // namespace Lkt::Query
