#pragma once

#include "geo/country.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Lkt::Geo
{
constexpr uint8_t NoCountry{ std::numeric_limits<uint8_t>::max() };

// The ipv4 is in host byte order, as in Query::SServerAddress.
uint8_t FindCountry(uint32_t ipv4);
uint8_t FindCountryByCode(std::string_view code);
SCountry GetCountry(uint8_t country);
size_t GetNumCountries();
} // namespace Lkt::Geo
