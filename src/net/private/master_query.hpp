#pragma once

#include "master_id.hpp"
#include "query/server_address.hpp"

namespace Lkt::Net
{
struct SMasterQuery final
{
	SMasterId master;
	Query::SServerAddress address;
};
} // namespace Lkt::Net
