#include "geo/countries.hpp"
#include "country_table.hpp"
#include "embedded_ranges.hpp"
#include "range_table.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <span>

namespace Lkt::Geo
{
//////////////////////////////////////////////////////////////////////////
uint8_t FindCountry(uint32_t ipv4)
{
	return FindInRanges(Embedded::Ipv4Countries, ipv4);
}

//////////////////////////////////////////////////////////////////////////
uint8_t FindCountryByCode(std::string_view code)
{
	std::span<SCountry const> const table{ GetCountryTable() };
	auto const it{ std::ranges::lower_bound(table, code, std::ranges::less{}, &SCountry::code) };

	return (it != table.end() && it->code == code) ? static_cast<uint8_t>(it - table.begin()) : NoCountry;
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
