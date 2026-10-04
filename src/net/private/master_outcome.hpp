#pragma once

#include "master_id.hpp"
#include <string>
#include <string_view>

namespace Lkt::Net
{
// An empty failure means the master answered.
struct SMasterOutcome final
{
	SMasterId master;
	std::string_view host;
	std::string failure;
};
} // namespace Lkt::Net
