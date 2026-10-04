#include "empty_catalog_fixture.hpp"
#include "query/game_catalog.hpp"
#include <span>

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
void CEmptyCatalogTest::SetUp()
{
	std::span<Query::SProtocolDefinition const> const protocols{ Query::GetProtocolCatalog() };
	std::span<Query::SGameDefinition const> const games{ Query::GetGameCatalog() };

	m_protocols.assign(protocols.begin(), protocols.end());
	m_games.assign(games.begin(), games.end());
	Query::TerminateGameCatalog();
	Query::InitializeGameCatalog(m_protocols, {});
}

//////////////////////////////////////////////////////////////////////////
void CEmptyCatalogTest::TearDown()
{
	Query::TerminateGameCatalog();
	Query::InitializeGameCatalog(m_protocols, m_games);
}
} // namespace Lkt::Fixtures
