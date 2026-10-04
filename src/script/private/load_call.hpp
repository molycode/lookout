#pragma once

#include "query/protocol_option.hpp"
#include "script/master_transport.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Script
{
// A function the script leaves out stays 0, which no registry reference is.
struct SLoadCall final
{
	std::string_view source;
	std::string chunkName;
	int masterStart{ 0 };
	int masterReceive{ 0 };
	int serverStart{ 0 };
	int serverReceive{ 0 };
	int serverFinish{ 0 };
	int states{ 0 };
	EMasterTransport masterTransport{ EMasterTransport::Udp };
	std::vector<Query::SProtocolOption> options;
	std::string unknownField;
	std::string problem;
};
} // namespace Lkt::Script
