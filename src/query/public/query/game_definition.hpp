#pragma once

#include "query/game.hpp"
#include "query/key_match.hpp"
#include "query/launch_hints.hpp"
#include "query/master_endpoint.hpp"
#include "query/mode_rule.hpp"
#include "query/protocol_family.hpp"
#include "query/server_keys.hpp"
#include "query/text_style.hpp"
#include <string>
#include <vector>

namespace Lkt::Query
{
struct SGameDefinition final
{
	EGame game{ NoGame };
	std::string key;
	std::string name;
	EProtocolFamily family{ EProtocolFamily::Quake2 };
	ETextStyle textStyle{ ETextStyle::Ascii7 };
	std::vector<SMasterEndpoint> masters;
	std::string masterQueryArgs;
	SServerKeys keys;
	std::vector<SModeRule> modes;
	std::vector<SKeyMatch> foreignServers;
	SLaunchHints launch;
};
} // namespace Lkt::Query
