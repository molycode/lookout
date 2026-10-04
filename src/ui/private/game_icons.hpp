#pragma once

#include "icon_level.hpp"
#include "query/game.hpp"
#include <tge/non_copyable.hpp>
#include <vector>

struct ImDrawList;
struct ImVec2;
struct SDL_Renderer;

namespace Lkt::Ui
{
class CGameIcons final : private Tge::SNoCopyNoMove
{
public:

	CGameIcons() = default;
	~CGameIcons() = default;

	void Initialize(SDL_Renderer* pRenderer);
	void Terminate();

	void Draw(ImDrawList* pDrawList, Query::EGame game, ImVec2 const& min, float size) const;

private:

	std::vector<std::vector<SIconLevel>> m_levels;
};

extern CGameIcons gGameIcons;
} // namespace Lkt::Ui
