#pragma once

#include "icon_level.hpp"
#include <tge/non_copyable.hpp>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct SDL_Renderer;

namespace Lkt
{
namespace Download
{
class CGameDownloads;
struct SGameOffer;
} // namespace Download

namespace Ui
{
// Lists lookout-games' games with what is installed, and downloads, updates and removes them. A window, not a popup,
// so the games it brings appear in the sidebar while it is still open.
class CDownloadWindow final : private Tge::SNoCopyNoMove
{
public:

	CDownloadWindow() = default;
	~CDownloadWindow() = default;

	// Without downloads there is nowhere to download to, and it stays closed.
	void Initialize(SDL_Renderer* pRenderer, Download::CGameDownloads* pDownloads, std::string_view version);
	void Terminate();

	void Open();
	void Draw();
	bool CanOpen() const;

private:

	void DrawStatus() const;
	void DrawNewerLookout(std::string_view newer) const;
	void DrawGames();
	void DrawGame(Download::SGameOffer const& offer, std::vector<std::string>& toDownload, std::vector<std::string>& toRemove);
	void DrawButtons();
	void DrawInstalledCount() const;
	void DrawRemoveAllPrompt();
	std::span<SIconLevel const> FindIcon(Download::SGameOffer const& offer);
	void DestroyIcons();
	bool CanDownload() const;
	std::string_view GetNewerLookout() const;
	bool HasAny(bool (*pIsChosen)(Download::SGameOffer const&)) const;
	std::vector<std::string> Collect(bool (*pIsChosen)(Download::SGameOffer const&)) const;

	Download::CGameDownloads* m_pDownloads{ nullptr };
	std::map<std::string, std::vector<SIconLevel>, std::less<>> m_icons;
	std::string m_search;
	std::string m_version;
	SDL_Renderer* m_pRenderer{ nullptr };
	bool m_isOpen{ false };
	bool m_shouldFocus{ false };
	bool m_shouldFocusSearch{ false };
};
} // namespace Ui
} // namespace Lkt
