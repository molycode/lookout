#include "browser/text_compare.hpp"
#include <algorithm>

namespace Lkt::Browser
{
namespace
{
//////////////////////////////////////////////////////////////////////////
// As unsigned: UTF-8 bytes are negative as plain char.
unsigned char Lower(char character)
{
	unsigned char const byte{ static_cast<unsigned char>(character) };

	return (byte >= 'A' && byte <= 'Z') ? static_cast<unsigned char>(byte - 'A' + 'a') : byte;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool ContainsIgnoringCase(std::string_view text, std::string_view needle)
{
	return needle.empty() || !std::ranges::search(text, needle, {}, Lower, Lower).empty();
}

//////////////////////////////////////////////////////////////////////////
bool EqualsIgnoringCase(std::string_view lhs, std::string_view rhs)
{
	return std::ranges::equal(lhs, rhs, {}, Lower, Lower);
}

//////////////////////////////////////////////////////////////////////////
std::weak_ordering CompareIgnoringCase(std::string_view lhs, std::string_view rhs)
{
	return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char left, char right) -> std::weak_ordering
	{
		return Lower(left) <=> Lower(right);
	});
}
} // namespace Lkt::Browser
