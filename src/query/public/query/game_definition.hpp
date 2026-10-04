#pragma once

#include "query/game.hpp"
#include "query/join_command.hpp"
#include "query/key_match.hpp"
#include "query/launch_hints.hpp"
#include "query/master_endpoint.hpp"
#include "query/mode_rule.hpp"
#include "query/protocol.hpp"
#include "query/server_keys.hpp"
#include "query/text_style.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Lkt::Query
{
struct SGameDefinition final
{
	EGame game{ NoGame };
	std::string key;
	std::string name;
	EProtocol protocol{ NoProtocol };
	STextStyle text;
	std::vector<SMasterEndpoint> masters;
	std::map<std::string, std::string> protocolOptions;
	int32_t queryPortOffset{ 0 };
	SServerKeys keys;
	std::vector<SModeRule> modes;
	std::vector<SKeyMatch> foreignServers;
	SLaunchHints launch;
	SJoinCommand join;
	std::vector<std::byte> icon;

	bool operator==(SGameDefinition const&) const = default;
};
} // namespace Lkt::Query
