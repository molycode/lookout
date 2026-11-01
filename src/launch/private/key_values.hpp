#pragma once

#include "key_values_error.hpp"
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
struct SKeyValue final
{
	std::string path{};
	std::string value{};
};

std::expected<std::vector<SKeyValue>, EKeyValuesError> ParseKeyValues(std::string_view text);
} // namespace Lkt::Launch
