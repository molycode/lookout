#pragma once

// The only include of nlohmann/json. Without exceptions it aborts on any unchecked read; this logs why first.
// Never brace-initialise a json from a single value: nlohmann makes that a one-element array.

namespace Lkt::Json
{
[[noreturn]] void AbortOnJsonError(char const* pWhat);
} // namespace Lkt::Json

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#define JSON_THROW_USER(exception) ::Lkt::Json::AbortOnJsonError((exception).what())

#if defined(_MSC_VER) && !defined(__clang__)
	#pragma warning(push)
	#pragma warning(disable: 4702)
#endif // _MSC_VER && !__clang__
#include <nlohmann/json.hpp>
#if defined(_MSC_VER) && !defined(__clang__)
	#pragma warning(pop)
#endif // _MSC_VER && !__clang__
