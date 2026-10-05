#pragma once

#include "icon_level.hpp"
#include <cstddef>
#include <span>
#include <vector>

struct ImDrawList;
struct ImVec2;
struct SDL_Renderer;

namespace Lkt::Ui
{
std::vector<SIconLevel> LoadIconLevels(SDL_Renderer* pRenderer, std::span<std::byte const> png);
void DestroyIconLevels(std::vector<SIconLevel>& levels);
void DrawIcon(ImDrawList* pDrawList, std::span<SIconLevel const> levels, ImVec2 const& min, float size);
} // namespace Lkt::Ui
