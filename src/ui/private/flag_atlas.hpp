#pragma once

#include <tge/non_copyable.hpp>
#include <cstdint>

struct ImDrawList;
struct ImVec2;
struct SDL_Renderer;
struct SDL_Texture;

namespace Lkt::Ui
{
class CFlagAtlas final : private Tge::SNoCopyNoMove
{
public:

	CFlagAtlas() = default;
	~CFlagAtlas() = default;

	void Initialize(SDL_Renderer* pRenderer);
	void Terminate();

	static float GetWidth();
	// Centred on a text line starting at lineStart; nothing for Geo::NoCountry, or when the atlas did not load.
	void Draw(ImDrawList* pDrawList, uint8_t country, ImVec2 const& lineStart) const;
	// As an item at the cursor, so it can carry a tooltip.
	void DrawItem(uint8_t country) const;

private:

	SDL_Texture* m_pTexture{ nullptr };
	float m_textureWidth{ 0.0f };
	float m_textureHeight{ 0.0f };
};

extern CFlagAtlas gFlagAtlas;
} // namespace Lkt::Ui
