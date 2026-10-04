#pragma once

#include "query/protocol_option.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Script
{
struct SLoadCall final
{
	std::string_view source;
	std::string chunkName;
	int masterRequest{ 0 };
	int statusRequest{ 0 };
	int parseMasterReply{ 0 };
	int parseStatusReply{ 0 };
	std::vector<Query::SProtocolOption> options;
	std::string unknownField;
	std::string problem;
};
} // namespace Lkt::Script
