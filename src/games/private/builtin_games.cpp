#include "games/builtin_games.hpp"
#include "embedded_games.hpp"
#include "game_json.hpp"
#include "game_order.hpp"
#include "script/protocol_script.hpp"
#include <algorithm>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::string_view AsText(std::span<unsigned char const> bytes)
{
	return std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() };
}

//////////////////////////////////////////////////////////////////////////
void AddProtocol(std::string_view name, std::span<unsigned char const> bytes, Script::CProtocolScript& script, SBuiltins& builtins)
{
	std::expected<void, std::string> const loaded{ script.Initialize(name, AsText(bytes)) };

	if (loaded.has_value())
	{
		std::span<Query::SProtocolOption const> const options{ script.GetOptions() };

		builtins.protocols.emplace_back(Query::SProtocolDefinition{ std::string{ name }, std::string{ AsText(bytes) },
			std::vector<Query::SProtocolOption>{ options.begin(), options.end() } });
	}
	else
	{
		builtins.problems.emplace_back(std::format("The built-in protocol '{}' cannot be loaded: {}", name, loaded.error()));
	}
}

//////////////////////////////////////////////////////////////////////////
// The requests are built here, once: the network thread then only sends them.
void AddGame(std::string_view key, std::span<unsigned char const> bytes, std::span<Script::CProtocolScript> scripts, SBuiltins& builtins)
{
	std::expected<Query::SGameDefinition, std::string> game{ ReadGameJson(AsText(bytes), builtins.protocols) };

	if (game.has_value())
	{
		Script::CProtocolScript& script{ scripts[static_cast<size_t>(game->protocol)] };
		std::expected<std::vector<std::byte>, std::string> masterRequest{ script.MasterRequest(game->protocolOptions) };
		std::expected<std::vector<std::byte>, std::string> statusRequest{ script.StatusRequest(game->protocolOptions) };

		if (masterRequest.has_value() && statusRequest.has_value())
		{
			game->key = key;
			game->masterRequest = std::move(*masterRequest);
			game->statusRequest = std::move(*statusRequest);
			builtins.games.emplace_back(std::move(*game));
		}
		else
		{
			game = std::unexpected{ masterRequest.has_value() ? statusRequest.error() : masterRequest.error() };
		}
	}

	if (!game.has_value())
	{
		builtins.problems.emplace_back(std::format("The built-in game '{}' cannot be read: {}", key, game.error()));
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SBuiltins LoadBuiltins()
{
	SBuiltins builtins{};
	std::vector<Script::CProtocolScript> scripts(Embedded::Protocols.size());

	for (size_t index{ 0 }; index < Embedded::Protocols.size(); ++index)
	{
		std::string_view const name{ Embedded::Protocols[index].name };

		AddProtocol(name.substr(0, name.rfind('.')), Embedded::Protocols[index].bytes, scripts[index], builtins);
	}

	std::expected<std::vector<std::string>, std::string> const order{ ReadGameOrder(AsText(Embedded::GameOrder)) };

	if (order.has_value())
	{
		for (Embedded::SEmbeddedFile const& file : Embedded::Games)
		{
			std::string_view const key{ file.name.substr(0, file.name.find('/')) };

			if (!std::ranges::contains(*order, key))
			{
				builtins.problems.emplace_back(std::format("The built-in game '{}' is missing from order.json", key));
			}
		}
	}
	else
	{
		builtins.problems.emplace_back(std::format("The built-in game order cannot be read: {}", order.error()));
	}

	// Games name protocols by position, which holds only when every protocol loaded.
	if (builtins.problems.empty())
	{
		for (std::string const& key : *order)
		{
			std::string const path{ std::format("{}/game.json", key) };
			auto const file{ std::ranges::find(Embedded::Games, std::string_view{ path }, &Embedded::SEmbeddedFile::name) };

			if (file != Embedded::Games.end())
			{
				AddGame(key, file->bytes, scripts, builtins);
			}
			else
			{
				builtins.problems.emplace_back(std::format("order.json lists '{}', which has no game.json", key));
			}
		}
	}

	for (Script::CProtocolScript& script : scripts)
	{
		script.Terminate();
	}

	return builtins;
}
} // namespace Lkt::Games
