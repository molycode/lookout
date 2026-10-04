#include "launch/describe_password_rules.hpp"
#include <format>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
std::string DescribePasswordRules(Query::SPasswordRules const& rules)
{
	std::vector<std::string> refused{};
	std::string text{ std::format("up to {} printable ASCII characters", rules.maxLength) };

	for (char const character : rules.refusedCharacters)
	{
		refused.emplace_back((character == ' ') ? std::string{ "spaces" } : std::string(1, character));
	}

	refused.insert(refused.end(), rules.refusedSequences.begin(), rules.refusedSequences.end());

	for (size_t index{ 0 }; index < refused.size(); ++index)
	{
		std::string_view const separator{ (index == 0) ? ", without " : (index + 1 == refused.size()) ? " or " : ", " };

		text += std::format("{}{}", separator, refused[index]);
	}

	return text;
}
} // namespace Lkt::Launch
