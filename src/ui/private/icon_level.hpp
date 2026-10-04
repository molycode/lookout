#pragma once

struct SDL_Texture;

namespace Lkt::Ui
{
struct SIconLevel final
{
	SDL_Texture* pTexture{ nullptr };
	float size{ 0.0f };
};
} // namespace Lkt::Ui
