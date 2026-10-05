#include "icon_textures.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <cfloat>
#include <cstdint>

namespace Lkt::Ui
{
namespace
{
constexpr int MinLevelSize{ 16 };
constexpr float StandInScale{ 0.75f };
} // namespace

//////////////////////////////////////////////////////////////////////////
// Premultiplied, so shrinking never darkens the transparent edges.
std::vector<SIconLevel> LoadIconLevels(SDL_Renderer* pRenderer, std::span<std::byte const> png)
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
		DestroyIconLevels(levels);
	}

	return levels;
}

//////////////////////////////////////////////////////////////////////////
void DestroyIconLevels(std::vector<SIconLevel>& levels)
{
	for (SIconLevel const& level : levels)
	{
		SDL_DestroyTexture(level.pTexture);
	}

	levels.clear();
}

//////////////////////////////////////////////////////////////////////////
void DrawIcon(ImDrawList* pDrawList, std::span<SIconLevel const> levels, ImVec2 const& min, float size)
{
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
