#include "range_table.hpp"
#include "geo/countries.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <ranges>

namespace Lkt::Geo
{
namespace
{
constexpr size_t CountSize{ sizeof(uint32_t) };
constexpr size_t StartSize{ sizeof(uint32_t) };

//////////////////////////////////////////////////////////////////////////
uint32_t ReadUnsigned(std::span<unsigned char const> table, size_t offset)
{
	return static_cast<uint32_t>(table[offset]) | (static_cast<uint32_t>(table[offset + 1]) << 8)
		| (static_cast<uint32_t>(table[offset + 2]) << 16) | (static_cast<uint32_t>(table[offset + 3]) << 24);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
size_t GetNumRanges(std::span<unsigned char const> table)
{
	size_t numRanges{ 0 };

	if (table.size() >= CountSize)
	{
		size_t const count{ ReadUnsigned(table, 0) };

		if (table.size() == CountSize + count * (StartSize + sizeof(uint8_t)))
		{
			numRanges = count;
		}
	}

	TGE_ASSERT(numRanges != 0, "Malformed country range table");

	return numRanges;
}

//////////////////////////////////////////////////////////////////////////
uint32_t GetRangeStart(std::span<unsigned char const> table, size_t range)
{
	return ReadUnsigned(table, CountSize + range * StartSize);
}

//////////////////////////////////////////////////////////////////////////
uint8_t GetRangeCountry(std::span<unsigned char const> table, size_t range)
{
	return table[CountSize + GetNumRanges(table) * StartSize + range];
}

//////////////////////////////////////////////////////////////////////////
uint8_t FindInRanges(std::span<unsigned char const> table, uint32_t ipv4)
{
	auto const ranges{ std::views::iota(size_t{ 0 }, GetNumRanges(table)) };
	auto const after{ std::ranges::upper_bound(ranges, ipv4, std::ranges::less{},
		[table](size_t range) { return GetRangeStart(table, range); }) };
	size_t const numBefore{ static_cast<size_t>(after - ranges.begin()) };

	return (numBefore != 0) ? GetRangeCountry(table, numBefore - 1) : NoCountry;
}
} // namespace Lkt::Geo
