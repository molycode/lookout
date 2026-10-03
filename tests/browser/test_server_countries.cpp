#include "server_countries.hpp"
#include "geo/countries.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Lkt::Browser
{
namespace
{
//////////////////////////////////////////////////////////////////////////
SServerEntry MakeEntry(EServerState state, std::string_view code)
{
	SServerEntry entry{};

	entry.state = state;
	entry.country = Geo::FindCountryByCode(code);

	return entry;
}

//////////////////////////////////////////////////////////////////////////
// Switzerland's code sorts before Germany's, its name after.
TEST(ServerCountries, DistinctCountriesAreSortedByName)
{
	std::vector<SServerEntry> const entries{ MakeEntry(EServerState::Online, "US"), MakeEntry(EServerState::Online, "DE"),
		MakeEntry(EServerState::Online, "CH"), MakeEntry(EServerState::Online, "DE") };
	std::vector<uint8_t> countries{};

	CollectCountries(entries, countries);

	EXPECT_EQ(countries, (std::vector<uint8_t>{ Geo::FindCountryByCode("DE"), Geo::FindCountryByCode("CH"), Geo::FindCountryByCode("US") }));
}

//////////////////////////////////////////////////////////////////////////
TEST(ServerCountries, SilentServersAndUnknownCountriesAreLeftOut)
{
	std::vector<SServerEntry> const entries{ MakeEntry(EServerState::NoAnswer, "FR"), MakeEntry(EServerState::Online, "XX"),
		MakeEntry(EServerState::Online, "NL") };
	std::vector<uint8_t> countries{};

	CollectCountries(entries, countries);

	EXPECT_EQ(countries, std::vector<uint8_t>{ Geo::FindCountryByCode("NL") });
}
} // namespace
} // namespace Lkt::Browser
