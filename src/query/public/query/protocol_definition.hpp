#pragma once

#include "query/protocol_option.hpp"
#include "query/protocol_origin.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Lkt::Query
{
// A protocol script's source and the options it declares; each thread that runs it builds its own state.
struct SProtocolDefinition final
{
	std::string name;
	std::string source;
	std::vector<SProtocolOption> options;
	EProtocolOrigin origin{ EProtocolOrigin::Downloaded };
	std::optional<uint64_t> downloadedVersion{};

	bool operator==(SProtocolDefinition const&) const = default;
};
} // namespace Lkt::Query
