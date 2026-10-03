#pragma once

// The only include of nlohmann/json. Without exceptions it aborts on any unchecked read; this logs why first.
// Never brace-initialise a json from a single value: nlohmann makes that a one-element array.

namespace Lkt::Config
{
[[noreturn]] void AbortOnJsonError(char const* pWhat);
} // namespace Lkt::Config

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#define JSON_THROW_USER(exception) ::Lkt::Config::AbortOnJsonError((exception).what())

#include <nlohmann/json.hpp>
