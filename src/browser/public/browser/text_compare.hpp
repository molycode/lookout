#pragma once

#include <compare>
#include <string_view>

namespace Lkt::Browser
{
// ASCII case only: decoded names are UTF-8, and other letters compare as they are.
bool ContainsIgnoringCase(std::string_view text, std::string_view needle);
bool EqualsIgnoringCase(std::string_view lhs, std::string_view rhs);
// Weak: "Same" and "same" are equivalent without being equal.
std::weak_ordering CompareIgnoringCase(std::string_view lhs, std::string_view rhs);
} // namespace Lkt::Browser
