#include "range_table.hpp"
#include "geo/countries.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace Lkt::Geo
{
namespace
{
//////////////////////////////////////////////////////////////////////////
void AppendUnsigned(std::vector<unsigned char>& bytes, uint32_t value)
{
	for (uint32_t shift{ 0 }; shift < 32; shift += 8)
	{
		bytes.push_back(static_cast<unsigned char>(value >> shift));
	}
}

//////////////////////////////////////////////////////////////////////////
std::vector<unsigned char> MakeTable(std::initializer_list<std::pair<uint32_t, uint8_t>> ranges)
{
	std::vector<unsigned char> table{};

	AppendUnsigned(table, static_cast<uint32_t>(ranges.size()));

	for (std::pair<uint32_t, uint8_t> const& range : ranges)
	{
		AppendUnsigned(table, range.first);
	}

	for (std::pair<uint32_t, uint8_t> const& range : ranges)
	{
		table.push_back(range.second);
	}

	return table;
}

//////////////////////////////////////////////////////////////////////////
std::vector<unsigned char> MakeSampleTable()
{
	return MakeTable({ { 0x00000000, NoCountry }, { 0x01000000, 3 }, { 0x02000000, NoCountry }, { 0x08080000, 7 } });
}

//////////////////////////////////////////////////////////////////////////
TEST(RangeTable, FirstAddressOfARangeBelongsToIt)
{
	EXPECT_EQ(FindInRanges(MakeSampleTable(), 0x01000000), 3u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RangeTable, LastAddressOfARangeBelongsToIt)
{
	EXPECT_EQ(FindInRanges(MakeSampleTable(), 0x01FFFFFF), 3u);
}

//////////////////////////////////////////////////////////////////////////
TEST(RangeTable, UnassignedRangeHasNoCountry)
{
	EXPECT_EQ(FindInRanges(MakeSampleTable(), 0x02000000), NoCountry);
}

//////////////////////////////////////////////////////////////////////////
TEST(RangeTable, FirstRangeStartsAtZero)
{
	EXPECT_EQ(FindInRanges(MakeSampleTable(), 0x00000000), NoCountry);
}

//////////////////////////////////////////////////////////////////////////
TEST(RangeTable, LastRangeRunsToTheEnd)
{
	EXPECT_EQ(FindInRanges(MakeSampleTable(), 0xFFFFFFFF), 7u);
}
} // namespace
} // namespace Lkt::Geo
