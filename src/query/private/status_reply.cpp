#include "query/status_reply.hpp"
#include <algorithm>

namespace Lkt::Query
{
//////////////////////////////////////////////////////////////////////////
std::string_view FindRule(SStatusReply const& reply, std::string_view key)
{
	auto const it{ std::ranges::find(reply.rules, key, &SRule::key) };

	return (it != reply.rules.end()) ? std::string_view{ it->value } : std::string_view{};
}
} // namespace Lkt::Query
