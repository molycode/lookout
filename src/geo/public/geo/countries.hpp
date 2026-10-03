#pragma once

#include "geo/country.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Lkt::Geo
{
constexpr uint8_t NoCountry{ std::numeric_limits<uint8_t>::max() };

// The ipv4 is in host byte order, as in Query::SServerAddress.
uint8_t FindCountry(uint32_t ipv4);
SCountry GetCountry(uint8_t country);
size_t GetNumCountries();
} // namespace Lkt::Geo
