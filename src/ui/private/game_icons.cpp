#include "game_icons.hpp"
#include "icons.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Lkt::Ui
{
namespace
{
constexpr int MinLevelSize{ 16 };
constexpr float StandInScale{ 0.75f };

//////////////////////////////////////////////////////////////////////////
void DestroyLevels(std::vector<SIconLevel>& levels)
{
	for (SIconLevel const& level : levels)
	{
		SDL_DestroyTexture(level.pTexture);
	}

	levels.clear();
}

//////////////////////////////////////////////////////////////////////////
// Premultiplied, so shrinking never darkens the transparent edges.
std::vector<SIconLevel> LoadLevels(SDL_Renderer* pRenderer, std::span<std::byte const> png)
{
	std::vector<SIconLevel> levels{};
	SDL_IOStream* const pStream{ SDL_IOFromConstMem(png.data(), png.size()) };
	SDL_Surface* const pDecoded{ (pStream != nullptr) ? SDL_LoadPNG_IO(pStream, true) : nullptr };
	SDL_Surface* pSurface{ (pDecoded != nullptr) ? SDL_ConvertSurface(pDecoded, SDL_PIXELFORMAT_RGBA32) : nullptr };
	bool isLoaded{ pSurface != nullptr && SDL_PremultiplySurfaceAlpha(pSurface, false) };

	SDL_DestroySurface(pDecoded);

	while (isLoaded && pSurface != nullptr && pSurface->w >= MinLevelSize)
	{
		SDL_Texture* const pTexture{ SDL_CreateTextureFromSurface(pRenderer, pSurface) };

		isLoaded = pTexture != nullptr && SDL_SetTextureBlendMode(pTexture, SDL_BLENDMODE_BLEND_PREMULTIPLIED)
			&& SDL_SetTextureScaleMode(pTexture, SDL_SCALEMODE_LINEAR);

		if (pTexture != nullptr)
		{
			levels.emplace_back(pTexture, static_cast<float>(pSurface->w));
		}

		SDL_Surface* const pHalf{ isLoaded ? SDL_ScaleSurface(pSurface, pSurface->w / 2, pSurface->h / 2, SDL_SCALEMODE_LINEAR) : nullptr };

		SDL_DestroySurface(pSurface);
		pSurface = pHalf;
	}

	SDL_DestroySurface(pSurface);

	if (!isLoaded)
	{
		DestroyLevels(levels);
	}

	return levels;
}
} // namespace

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

			levels = LoadLevels(pRenderer, game.icon);

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
		DestroyLevels(levels);
	}

	m_levels.clear();
}

//////////////////////////////////////////////////////////////////////////
void CGameIcons::Draw(ImDrawList* pDrawList, Query::EGame game, ImVec2 const& min, float size) const
{
	size_t const index{ static_cast<size_t>(game) };
	std::span<SIconLevel const> const levels{ (index < m_levels.size()) ? std::span<SIconLevel const>{ m_levels[index] } : std::span<SIconLevel const>{} };

	if (!levels.empty())
	{
		float const pixels{ size * ImGui::GetIO().DisplayFramebufferScale.y };
		SIconLevel const* pLevel{ &levels.front() };

		for (SIconLevel const& level : levels)
		{
			if (level.size >= pixels)
			{
				pLevel = &level;
			}
		}

		pDrawList->AddImage(ImTextureRef{ static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(pLevel->pTexture)) }, min, ImVec2{ min.x + size, min.y + size });
	}
	else
	{
		ImFont* const pFont{ ImGui::GetFont() };
		float const fontSize{ size * StandInScale };
		ImVec2 const glyph{ pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, LKT_ICON_GAMEPAD) };

		pDrawList->AddText(pFont, fontSize, ImVec2{ min.x + (size - glyph.x) * 0.5f, min.y + (size - glyph.y) * 0.5f },
			ImGui::GetColorU32(GetThemeColors().textDisabled), LKT_ICON_GAMEPAD);
	}
}
} // namespace Lkt::Ui
