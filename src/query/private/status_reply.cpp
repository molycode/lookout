#include "query/status_reply.hpp"
#include <algorithm>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
char ToLowerAscii(char c)
{
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// The engines look rules up with Q_stricmp, and SoF2Plus servers send sv_maxClients.
std::string_view FindRule(SStatusReply const& reply, std::string_view key)
{
	auto const it{ std::ranges::find_if(reply.rules, [key](SRule const& rule) { return std::ranges::equal(rule.key, key, {}, ToLowerAscii, ToLowerAscii); }) };

	return (it != reply.rules.end()) ? std::string_view{ it->value } : std::string_view{};
}
} // namespace Lkt::Query
