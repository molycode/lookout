#pragma once

#include "config/window_settings.hpp"
#include "ui/about_info.hpp"
#include <tge/non_copyable.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Ui
{
class CMainWindow;

class CApplication final : private Tge::SNoCopyNoMove
{
public:

	CApplication() = default;
	~CApplication() = default;

	// Game descriptions are edited in userDir; empty, they cannot be.
	bool Initialize(SAboutInfo const& about, Config::SWindowSettings const& window, std::filesystem::path const& userDir);
	std::function<void()> MakeWakeCallback() const;
	std::function<void()> MakeReloadCallback();
	void Run(Browser::CBrowser& browser, std::function<void()> const& reloadGames);
	Config::SWindowSettings GetWindowSettings() const;
	void Terminate();

private:

	bool CreateWindowAndRenderer(Config::SWindowSettings const& window);
	bool InitializeImGui();
	bool LoadFonts();
	bool ShowMainWindow();
	void ProcessEvent(SDL_Event const& event);
	void TrackNormalSize();
	void UpdateScale();
	void ApplyMinimumSize();
	void DrawFrame(Browser::CBrowser& browser, CMainWindow& mainWindow);
	void DrawMainWindow(Browser::CBrowser& browser, CMainWindow& mainWindow);
	bool CanReload() const;
	void ReloadGames(CMainWindow& mainWindow, std::function<void()> const& reloadGames);

	SDL_Window* m_pWindow{ nullptr };
	SDL_Renderer* m_pRenderer{ nullptr };
	Config::SWindowSettings m_window;
	SAboutInfo m_about;
	std::filesystem::path m_userDir;
	float m_scale{ 1.0f };
	uint32_t m_wakeEventType{ 0 };
	bool m_hasVsync{ false };
	bool m_isImGuiInitialized{ false };
	bool m_hasReportedRenderFailure{ false };
	std::chrono::steady_clock::time_point m_activeUntil{};
	std::atomic<bool> m_isReloadRequested{ false };
};
} // namespace Ui
} // namespace Lkt
