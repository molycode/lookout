#pragma once

#include "geo/country.hpp"
#include <span>

namespace Lkt::Geo
{
std::span<SCountry const> GetCountryTable();
} // namespace Lkt::Geo
