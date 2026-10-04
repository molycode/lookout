#pragma once

#include "download/game_downloads.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
	void Initialize(std::filesystem::path const& userDir, std::string_view version, std::function<void()> wake);
	void Terminate();

	void Open();
	// Every frame, open or not: true when the downloaded games changed, so they are to be loaded again.
	bool Update();
	void Draw();
	bool CanOpen() const;

private:

	void DrawStatus() const;
	void DrawGames();
	void DrawButtons();
	void StartDownload(std::vector<std::string> const& keys);
	bool HasAny(std::span<Download::EOfferState const> states, bool isSelectedOnly) const;
	std::vector<std::string> Collect(std::span<Download::EOfferState const> states, bool isSelectedOnly) const;

	Download::CGameDownloads m_downloads;
	std::set<std::string> m_selected;
	bool m_isReady{ false };
	bool m_isOpen{ false };
	bool m_shouldFocus{ false };
};
} // namespace Lkt::Ui
