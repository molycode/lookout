#pragma once

#include <cstdint>

namespace Lkt::Query
{
enum class EStoreRegistryRoot : uint8_t
{
	CurrentUser,
	LocalMachine
};
} // namespace Lkt::Query
