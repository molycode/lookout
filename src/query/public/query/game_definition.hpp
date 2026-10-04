#pragma once

#include "query/game.hpp"
#include "query/key_match.hpp"
#include "query/launch_hints.hpp"
#include "query/master_endpoint.hpp"
#include "query/mode_rule.hpp"
#include "query/protocol.hpp"
#include "query/server_keys.hpp"
#include "query/text_style.hpp"
#include <cstddef>
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
	ETextStyle textStyle{ ETextStyle::Ascii7 };
	std::vector<SMasterEndpoint> masters;
	std::map<std::string, std::string> protocolOptions;
	SServerKeys keys;
	std::vector<SModeRule> modes;
	std::vector<SKeyMatch> foreignServers;
	SLaunchHints launch;
	// Built once from the protocol options, when the game is loaded.
	std::vector<std::byte> masterRequest;
	std::vector<std::byte> statusRequest;
};
} // namespace Lkt::Query
