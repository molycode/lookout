#include "download/lookout_release.hpp"
#include <charconv>
#include <cstddef>
#include <system_error>

namespace Lkt::Download
{
//////////////////////////////////////////////////////////////////////////
// Three whole numbers joined by dots, nothing before or after: compared as an array, 1.10.0 comes after 1.9.0.
std::optional<std::array<uint32_t, 3>> ParseLookoutVersion(std::string_view text)
{
	std::array<uint32_t, 3> numbers{};
	char const* pNext{ text.data() };
	char const* const pEnd{ text.data() + text.size() };
	bool isValid{ true };

	for (size_t index{ 0 }; isValid && index < numbers.size(); ++index)
	{
		bool const isSeparated{ index == 0 || (pNext != pEnd && *pNext == '.') };

		pNext += (index > 0 && isSeparated) ? 1 : 0;

		std::from_chars_result const result{ isSeparated ? std::from_chars(pNext, pEnd, numbers[index]) : std::from_chars_result{ pNext, std::errc::invalid_argument } };

		isValid = isSeparated && result.ec == std::errc{};
		pNext = result.ptr;
	}

	return (isValid && pNext == pEnd) ? std::optional<std::array<uint32_t, 3>>{ numbers } : std::nullopt;
}
} // namespace Lkt::Download
