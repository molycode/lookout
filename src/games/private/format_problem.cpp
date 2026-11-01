#include "format_problem.hpp"
#include "games/game_format.hpp"
#include <format>

namespace Lkt::Games
{
namespace
{
// Masters with protocols of their own; 1.5 dropped them, so no Lookout reads this format any more.
constexpr uint64_t SpentGameFormat{ 2 };
} // namespace

//////////////////////////////////////////////////////////////////////////
// A phrase for "format: <reason>", or nothing for a format this Lookout reads.
std::optional<std::string> DescribeUnreadableFormat(uint64_t format)
{
	std::optional<std::string> reason{};

	if (format == SpentGameFormat)
	{
		reason = std::format("is {}, which only Lookout 1.4 read", format);
	}
	else if (format > GameFormat)
	{
		reason = std::format("is {}, which needs a newer Lookout", format);
	}
	else if (!IsReadableGameFormat(format))
	{
		reason = std::format("must be {} or {}", FirstGameFormat, GameFormat);
	}

	return reason;
}
} // namespace Lkt::Games
