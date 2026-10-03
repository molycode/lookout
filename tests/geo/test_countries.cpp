#include "geo/countries.hpp"
#include "embedded_ranges.hpp"
#include "range_table.hpp"
#include <gtest/gtest.h>
#include <set>
#include <string_view>

namespace Lkt::Geo
{
namespace
{
//////////////////////////////////////////////////////////////////////////
TEST(Countries, EmbeddedRangesStartAtZero)
{
	ASSERT_NE(GetNumRanges(Embedded::Ipv4Countries), 0u);
	EXPECT_EQ(GetRangeStart(Embedded::Ipv4Countries, 0), 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, EmbeddedRangeStartsAscend)
{
	size_t const numRanges{ GetNumRanges(Embedded::Ipv4Countries) };
	size_t numUnordered{ 0 };

	for (size_t range{ 1 }; range < numRanges; ++range)
	{
		if (GetRangeStart(Embedded::Ipv4Countries, range - 1) >= GetRangeStart(Embedded::Ipv4Countries, range))
		{
			++numUnordered;
		}
	}

	EXPECT_EQ(numUnordered, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, EmbeddedRangesNameKnownCountries)
{
	size_t const numRanges{ GetNumRanges(Embedded::Ipv4Countries) };
	size_t numUnknown{ 0 };

	for (size_t range{ 0 }; range < numRanges; ++range)
	{
		uint8_t const country{ GetRangeCountry(Embedded::Ipv4Countries, range) };

		if (country != NoCountry && country >= GetNumCountries())
		{
			++numUnknown;
		}
	}

	EXPECT_EQ(numUnknown, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, CodesAreUniqueUpperCasePairs)
{
	std::set<std::string_view> codes{};

	for (size_t country{ 0 }; country < GetNumCountries(); ++country)
	{
		std::string_view const code{ GetCountry(static_cast<uint8_t>(country)).code };

		EXPECT_TRUE(code.size() == 2 && code[0] >= 'A' && code[0] <= 'Z' && code[1] >= 'A' && code[1] <= 'Z') << code;
		EXPECT_TRUE(codes.insert(code).second) << code;
	}
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, CodesAscend)
{
	size_t numUnordered{ 0 };

	for (size_t country{ 1 }; country < GetNumCountries(); ++country)
	{
		if (GetCountry(static_cast<uint8_t>(country - 1)).code >= GetCountry(static_cast<uint8_t>(country)).code)
		{
			++numUnordered;
		}
	}

	EXPECT_EQ(numUnordered, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, EveryCountryHasAName)
{
	size_t numUnnamed{ 0 };

	for (size_t country{ 0 }; country < GetNumCountries(); ++country)
	{
		if (GetCountry(static_cast<uint8_t>(country)).name.empty())
		{
			++numUnnamed;
		}
	}

	EXPECT_EQ(numUnnamed, 0u);
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, PublicResolverIsFound)
{
	uint8_t const country{ FindCountry(0x08080808) };

	ASSERT_NE(country, NoCountry);
	EXPECT_EQ(GetCountry(country).code, "US");
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, CodeFindsItsCountry)
{
	uint8_t const country{ FindCountryByCode("DE") };

	ASSERT_NE(country, NoCountry);
	EXPECT_EQ(GetCountry(country).name, "Germany");
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, UnknownCodeHasNoCountry)
{
	EXPECT_EQ(FindCountryByCode("XX"), NoCountry);
}

//////////////////////////////////////////////////////////////////////////
TEST(Countries, PrivateAddressHasNoCountry)
{
	EXPECT_EQ(FindCountry(0x0A000001), NoCountry);
}
} // namespace
} // namespace Lkt::Geo
