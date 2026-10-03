#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Lkt::Geo
{
// From tools/make-geo.py: a u32 count, each range start as a u32, then one country byte per range; little-endian.
size_t GetNumRanges(std::span<unsigned char const> table);
uint32_t GetRangeStart(std::span<unsigned char const> table, size_t range);
uint8_t GetRangeCountry(std::span<unsigned char const> table, size_t range);
uint8_t FindInRanges(std::span<unsigned char const> table, uint32_t ipv4);
} // namespace Lkt::Geo
