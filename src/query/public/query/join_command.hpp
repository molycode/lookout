#pragma once

#include "query/password_rules.hpp"
#include <string>
#include <vector>

namespace Lkt::Query
{
// Arguments appended to a launcher, with {address} and {password} replaced.
struct SJoinCommand final
{
	std::vector<std::string> arguments;
	std::vector<std::string> passwordArguments;
	SPasswordRules password;
};
} // namespace Lkt::Query
