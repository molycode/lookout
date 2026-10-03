#include "server_countries.hpp"
#include "browser/text_compare.hpp"
#include "geo/countries.hpp"
#include <algorithm>

namespace Lkt::Browser
{
//////////////////////////////////////////////////////////////////////////
void CollectCountries(std::span<SServerEntry const> entries, std::vector<uint8_t>& countries)
{
	countries.clear();

	for (SServerEntry const& entry : entries)
	{
		if (entry.state == EServerState::Online && entry.country != Geo::NoCountry)
		{
			countries.emplace_back(entry.country);
		}
	}

	std::ranges::sort(countries);
	auto const duplicates{ std::ranges::unique(countries) };

	countries.erase(duplicates.begin(), duplicates.end());
	std::ranges::sort(countries, [](uint8_t lhs, uint8_t rhs)
	{
		return CompareIgnoringCase(Geo::GetCountry(lhs).name, Geo::GetCountry(rhs).name) < 0;
	});
}
} // namespace Lkt::Browser
