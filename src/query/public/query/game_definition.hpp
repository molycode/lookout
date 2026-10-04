#pragma once

#include "query/game.hpp"
#include "query/key_match.hpp"
#include "query/launch_hints.hpp"
#include "query/master_endpoint.hpp"
#include "query/mode_rule.hpp"
#include "query/protocol_family.hpp"
#include "query/server_keys.hpp"
#include "query/text_style.hpp"
#include <span>
#include <string_view>

namespace Lkt::Query
{
struct SGameDefinition final
{
	EGame game{ NoGame };
	std::string_view key;
	std::string_view name;
	EProtocolFamily family{ EProtocolFamily::Quake2 };
	ETextStyle textStyle{ ETextStyle::Ascii7 };
	std::span<SMasterEndpoint const> masters;
	std::string_view masterQueryArgs;
	SServerKeys keys;
	std::span<SModeRule const> modes;
	std::span<SKeyMatch const> foreignServers;
	SLaunchHints launch;
};
} // namespace Lkt::Query
