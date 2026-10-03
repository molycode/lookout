#include "geo/countries.hpp"
#include "country_table.hpp"
#include "embedded_ranges.hpp"
#include "range_table.hpp"
#include <tge/assert.hpp>
#include <span>

namespace Lkt::Geo
{
//////////////////////////////////////////////////////////////////////////
uint8_t FindCountry(uint32_t ipv4)
{
	return FindInRanges(Embedded::Ipv4Countries, ipv4);
}

//////////////////////////////////////////////////////////////////////////
SCountry GetCountry(uint8_t country)
{
	std::span<SCountry const> const table{ GetCountryTable() };

	TGE_ASSERT(country < table.size(), "Country index outside the country table");

	return (country < table.size()) ? table[country] : SCountry{};
}

//////////////////////////////////////////////////////////////////////////
size_t GetNumCountries()
{
	return GetCountryTable().size();
}
} // namespace Lkt::Geo
