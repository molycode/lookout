#include "format_three_ids.hpp"
#include "query/store_ids.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <system_error>

namespace Lkt::Games
{
namespace
{
// Steam's then GOG's, the order lookout-games writes them in.
constexpr std::array<SFormatThreeId, 2> FormatThreeIds{ {
	{ Query::SteamStore, "steamAppId", std::numeric_limits<uint32_t>::max(), "from 1 to 4294967295" },
	{ Query::GogStore, "gogId", std::numeric_limits<uint64_t>::max(), "from 1" }
} };
} // namespace

//////////////////////////////////////////////////////////////////////////
std::span<SFormatThreeId const> GetFormatThreeIds()
{
	return FormatThreeIds;
}

//////////////////////////////////////////////////////////////////////////
SFormatThreeId const* FindFormatThreeId(std::string_view store)
{
	auto const it{ std::ranges::find(FormatThreeIds, store, &SFormatThreeId::store) };

	return (it != FormatThreeIds.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
// Digits alone, with no leading zero, so the text names exactly one number.
std::optional<uint64_t> ReadCanonicalDecimal(std::string_view text)
{
	uint64_t value{ 0 };
	std::from_chars_result const result{ std::from_chars(text.data(), text.data() + text.size(), value) };
	bool const isCanonical{ !text.empty() && text.front() != '0' && result.ec == std::errc{} && result.ptr == text.data() + text.size() };

	return isCanonical ? std::optional<uint64_t>{ value } : std::nullopt;
}
} // namespace Lkt::Games
