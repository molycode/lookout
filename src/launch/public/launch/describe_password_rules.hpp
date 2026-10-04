#pragma once

#include "query/password_rules.hpp"
#include <string>

namespace Lkt::Launch
{
std::string DescribePasswordRules(Query::SPasswordRules const& rules);
} // namespace Lkt::Launch
