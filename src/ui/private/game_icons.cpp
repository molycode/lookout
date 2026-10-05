#include "game_icons.hpp"
#include "icon_textures.hpp"
#include "loggers.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <SDL3/SDL.h>
#include <cstddef>
#include <span>

namespace Lkt::Ui
{
constinit CGameIcons gGameIcons{};

//////////////////////////////////////////////////////////////////////////
void CGameIcons::Initialize(SDL_Renderer* pRenderer)
{
	std::span<Query::SGameDefinition const> const catalog{ Query::GetGameCatalog() };

	m_levels.resize(catalog.size());

	for (Query::SGameDefinition const& game : catalog)
	{
		if (!game.icon.empty())
		{
			std::vector<SIconLevel>& levels{ m_levels[static_cast<size_t>(game.game)] };

			levels = LoadIconLevels(pRenderer, game.icon);

			if (levels.empty())
			{
				gLog.Warning("Cannot load the icon of {}, so a stand-in shows instead: {}", game.name, SDL_GetError());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameIcons::Terminate()
{
	for (std::vector<SIconLevel>& levels : m_levels)
	{
		DestroyIconLevels(levels);
	}

	m_levels.clear();
}

//////////////////////////////////////////////////////////////////////////
void CGameIcons::Draw(ImDrawList* pDrawList, Query::EGame game, ImVec2 const& min, float size) const
{
	size_t const index{ static_cast<size_t>(game) };
	std::span<SIconLevel const> const levels{ (index < m_levels.size()) ? std::span<SIconLevel const>{ m_levels[index] } : std::span<SIconLevel const>{} };

	DrawIcon(pDrawList, levels, min, size);
}
} // namespace Lkt::Ui
