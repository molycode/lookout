#pragma once

#include "licensed_component.hpp"
#include <span>

namespace Lkt::Ui
{
std::span<SLicensedComponent const> GetLicensedComponents();
} // namespace Lkt::Ui
