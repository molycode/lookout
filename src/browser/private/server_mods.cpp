#include "server_mods.hpp"
#include "browser/text_compare.hpp"
#include <algorithm>

namespace Lkt::Browser
{
//////////////////////////////////////////////////////////////////////////
void CollectMods(std::span<SServerEntry const> entries, std::vector<std::string>& mods)
{
	mods.clear();

	for (SServerEntry const& entry : entries)
	{
		if (entry.state == EServerState::Online && !entry.summary.mod.empty())
		{
			mods.emplace_back(entry.summary.mod);
		}
	}

	// Exact order breaks ties, so which spelling of a mod is kept does not depend on the order servers answered in.
	std::ranges::sort(mods, [](std::string const& lhs, std::string const& rhs)
	{
		std::weak_ordering const order{ CompareIgnoringCase(lhs, rhs) };

		return (order != 0) ? (order < 0) : (lhs < rhs);
	});

	auto const duplicates{ std::ranges::unique(mods, [](std::string const& lhs, std::string const& rhs) { return EqualsIgnoringCase(lhs, rhs); }) };

	mods.erase(duplicates.begin(), duplicates.end());
}
} // namespace Lkt::Browser
