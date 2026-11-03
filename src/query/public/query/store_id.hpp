#pragma once

#include <string>

namespace Lkt::Query
{
struct SStoreId final
{
	std::string store{};
	std::string id{};

	bool operator==(SStoreId const&) const = default;
};
} // namespace Lkt::Query
