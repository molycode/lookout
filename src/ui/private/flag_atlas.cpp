#include "flag_atlas.hpp"
#include "embedded_flags.hpp"
#include "loggers.hpp"
#include "geo/countries.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>

namespace Lkt::Ui
{
namespace
{
// tools/make-geo.py renders every flag into a cell of this size, in country order, row by row.
constexpr float CellWidth{ 32.0f };
constexpr float CellHeight{ 24.0f };
constexpr float LineFraction{ 0.75f };

//////////////////////////////////////////////////////////////////////////
float GetHeight()
{
	return ImGui::GetTextLineHeight() * LineFraction;
}
} // namespace

constinit CFlagAtlas gFlagAtlas{};

//////////////////////////////////////////////////////////////////////////
void CFlagAtlas::Initialize(SDL_Renderer* pRenderer)
{
	SDL_IOStream* const pStream{ SDL_IOFromConstMem(Embedded::Flags.data(), Embedded::Flags.size()) };
	SDL_Surface* const pSurface{ (pStream != nullptr) ? SDL_LoadPNG_IO(pStream, true) : nullptr };

	if (pSurface != nullptr)
	{
		m_pTexture = SDL_CreateTextureFromSurface(pRenderer, pSurface);
		SDL_DestroySurface(pSurface);
	}

	if (m_pTexture != nullptr && !SDL_GetTextureSize(m_pTexture, &m_textureWidth, &m_textureHeight))
	{
		SDL_DestroyTexture(m_pTexture);
		m_pTexture = nullptr;
	}

	if (m_pTexture == nullptr)
	{
		gLog.Error("Cannot load the flags, so servers show no flag: {}", SDL_GetError());
	}
	else if (!SDL_SetTextureScaleMode(m_pTexture, SDL_SCALEMODE_LINEAR))
	{
		gLog.Warning("Cannot smooth the flags, so they are drawn unfiltered: {}", SDL_GetError());
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlagAtlas::Terminate()
{
	if (m_pTexture != nullptr)
	{
		SDL_DestroyTexture(m_pTexture);
		m_pTexture = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
float CFlagAtlas::GetWidth()
{
	return GetHeight() * CellWidth / CellHeight;
}

//////////////////////////////////////////////////////////////////////////
void CFlagAtlas::Draw(ImDrawList* pDrawList, uint8_t country, ImVec2 const& lineStart) const
{
	if (m_pTexture != nullptr && country < Geo::GetNumCountries())
	{
		size_t const numColumns{ static_cast<size_t>(m_textureWidth / CellWidth) };
		float const column{ static_cast<float>(country % numColumns) };
		float const row{ static_cast<float>(country / numColumns) };
		float const height{ GetHeight() };
		ImVec2 const min{ lineStart.x, lineStart.y + (ImGui::GetTextLineHeight() - height) * 0.5f };
		// Half a texel in from the cell's edges, so linear filtering never blends in the neighbouring flag.
		ImVec2 const uvMin{ (column * CellWidth + 0.5f) / m_textureWidth, (row * CellHeight + 0.5f) / m_textureHeight };
		ImVec2 const uvMax{ ((column + 1.0f) * CellWidth - 0.5f) / m_textureWidth, ((row + 1.0f) * CellHeight - 0.5f) / m_textureHeight };

		pDrawList->AddImage(ImTextureRef{ static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(m_pTexture)) }, min,
			ImVec2{ min.x + GetWidth(), min.y + height }, uvMin, uvMax);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlagAtlas::DrawItem(uint8_t country) const
{
	ImVec2 const start{ ImGui::GetCursorScreenPos() };

	ImGui::Dummy(ImVec2{ GetWidth(), ImGui::GetTextLineHeight() });
	Draw(ImGui::GetWindowDrawList(), country, start);
}
} // namespace Lkt::Ui
