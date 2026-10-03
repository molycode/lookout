#pragma once

#include "config/sort_column.hpp"

namespace Lkt::Config
{
struct SSortOrder final
{
	ESortColumn column{ ESortColumn::Players };
	bool isAscending{ false };

	bool operator==(SSortOrder const&) const = default;
};
} // namespace Lkt::Config
