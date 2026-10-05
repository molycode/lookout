#pragma once

#include "icon_level.hpp"
#include "download/game_downloads.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct SDL_Renderer;

namespace Lkt::Ui
{
// Lists lookout-games' games with what is installed, and downloads, updates and removes them. A window, not a popup,
// so the games it brings appear in the sidebar while it is still open.
class CDownloadWindow final : private Tge::SNoCopyNoMove
{
public:

	CDownloadWindow() = default;
	~CDownloadWindow() = default;

	// Without a data folder there is nowhere to download to, and it stays closed.
	void Initialize(SDL_Renderer* pRenderer, std::filesystem::path const& userDir, std::string_view version, std::function<void()> wake);
	void Terminate();

	void Open();
	// Every frame, open or not: true when the downloaded games changed, so they are to be loaded again.
	bool Update();
	void Draw();
	bool CanOpen() const;

private:

	void DrawStatus() const;
	void DrawGames();
	void DrawGame(Download::SGameOffer const& offer, std::vector<std::string>& toDownload, std::vector<std::string>& toRemove);
	void DrawButtons();
	void DrawRemoveAllPrompt();
	std::span<SIconLevel const> FindIcon(Download::SGameOffer const& offer);
	bool CanDownload() const;
	bool HasAny(std::span<Download::EOfferState const> states) const;
	std::vector<std::string> Collect(std::span<Download::EOfferState const> states) const;

	Download::CGameDownloads m_downloads;
	std::map<std::string, std::vector<SIconLevel>, std::less<>> m_icons;
	std::string m_search;
	SDL_Renderer* m_pRenderer{ nullptr };
	bool m_isReady{ false };
	bool m_isOpen{ false };
	bool m_shouldFocus{ false };
	bool m_shouldFocusSearch{ false };
};
} // namespace Lkt::Ui
