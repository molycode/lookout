#include "games/builtin_games.hpp"
#include "embedded_games.hpp"
#include "game_json.hpp"
#include "loggers.hpp"
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace Lkt::Games
{
namespace
{
//////////////////////////////////////////////////////////////////////////
void AddBuiltinGame(std::string_view key, std::span<unsigned char const> bytes, std::vector<Query::SGameDefinition>& games)
{
	std::expected<Query::SGameDefinition, std::string> game{ ReadGameJson(std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() }) };

	if (game.has_value())
	{
		game->key = key;
		games.emplace_back(std::move(*game));
	}
	else
	{
		gLog.Error("The built-in game '{}' cannot be read: {}", key, game.error());
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::vector<Query::SGameDefinition> LoadBuiltinGames()
{
	std::vector<Query::SGameDefinition> games{};

	// The order also numbers each game's saved table layout, so a new game goes last.
	AddBuiltinGame("kingpin", Embedded::KingpinGame, games);
	AddBuiltinGame("quake2", Embedded::Quake2Game, games);
	AddBuiltinGame("rtcw", Embedded::RtcwGame, games);
	AddBuiltinGame("et", Embedded::EnemyTerritoryGame, games);
	AddBuiltinGame("quake3", Embedded::Quake3Game, games);

	return games;
}
} // namespace Lkt::Games
