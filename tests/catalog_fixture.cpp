#include "catalog_fixture.hpp"
#include "game_json.hpp"
#include "query/game_catalog.hpp"
#include "script/protocol_script.hpp"
#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
void CCatalogTest::SetUp()
{
	std::span<Query::SProtocolDefinition const> const protocols{ Query::GetProtocolCatalog() };
	std::span<Query::SGameDefinition const> const games{ Query::GetGameCatalog() };

	m_protocols.assign(protocols.begin(), protocols.end());
	m_games.assign(games.begin(), games.end());
	m_numLoadedProtocols = m_protocols.size();
	m_numLoadedGames = m_games.size();
}

//////////////////////////////////////////////////////////////////////////
void CCatalogTest::TearDown()
{
	m_protocols.resize(m_numLoadedProtocols);
	m_games.resize(m_numLoadedGames);
	Install();
}

//////////////////////////////////////////////////////////////////////////
Query::EProtocol CCatalogTest::AddProtocol(std::string_view name)
{
	std::filesystem::path const path{ std::filesystem::path{ LKT_TEST_SCRIPTS_DIR } / std::format("{}.lua", name) };
	std::ifstream file{ path, std::ios::binary };
	std::string const source{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
	Script::CProtocolScript script{};
	std::expected<void, std::string> const loaded{ script.Initialize(name, source) };

	EXPECT_TRUE(file.is_open()) << path;
	EXPECT_TRUE(loaded.has_value()) << loaded.error_or("");

	std::span<Query::SProtocolOption const> const options{ script.GetOptions() };

	m_protocols.emplace_back(std::string{ name }, source, std::vector<Query::SProtocolOption>{ options.begin(), options.end() });
	script.Terminate();
	Install();

	return static_cast<Query::EProtocol>(m_protocols.size() - 1);
}

//////////////////////////////////////////////////////////////////////////
Query::EGame CCatalogTest::AddGame(Query::SGameDefinition game)
{
	m_games.emplace_back(std::move(game));
	Install();

	return static_cast<Query::EGame>(m_games.size() - 1);
}

//////////////////////////////////////////////////////////////////////////
Query::EGame CCatalogTest::AddGameFile(std::string_view key, std::span<uint16_t const> masterPorts)
{
	std::filesystem::path const path{ std::filesystem::path{ LKT_TEST_GAMES_DIR } / key / "game.json" };
	std::ifstream file{ path, std::ios::binary };
	std::string const text{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
	std::expected<Query::SGameDefinition, std::string> game{ Games::ReadGameJson(text, m_protocols) };

	EXPECT_TRUE(file.is_open()) << path;
	EXPECT_TRUE(game.has_value()) << game.error_or("");
	EXPECT_EQ(game.has_value() ? game->masters.size() : masterPorts.size(), masterPorts.size());

	Query::SGameDefinition definition{ game.value_or(Query::SGameDefinition{}) };

	definition.key = key;

	for (size_t index{ 0 }; index < std::min(definition.masters.size(), masterPorts.size()); ++index)
	{
		definition.masters[index].port = masterPorts[index];
	}

	return AddGame(std::move(definition));
}

//////////////////////////////////////////////////////////////////////////
void CCatalogTest::Install()
{
	Query::TerminateGameCatalog();
	Query::InitializeGameCatalog(m_protocols, m_games);
}
} // namespace Lkt::Fixtures
