#include "ui/application.hpp"
#include "embedded_fonts.hpp"
#include "file_dialog.hpp"
#include "flag_atlas.hpp"
#include "game_icons.hpp"
#include "loggers.hpp"
#include "main_window.hpp"
#include "theme.hpp"
#include "browser/browser.hpp"
#include "config/window_limits.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <tge/assert.hpp>
#include <tge/module/runtime.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Lkt::Ui
{
namespace
{
constexpr float ScaleTolerance{ 0.01f };

// Font Awesome also maps glyphs onto Latin code points; only its private-use icons may fill in for the text font.
constexpr std::array<ImWchar, 5> IconExcludedRanges{ 0x0001, 0xDFFF, 0xF900, 0xFFFF, 0 };

// ImGui settles hover and focus over a few frames after the input that changed them.
constexpr std::chrono::milliseconds ActiveDuration{ 250 };
constexpr std::chrono::milliseconds UnsyncedFrameInterval{ 16 };
constexpr std::chrono::milliseconds DialogPollInterval{ 100 };
constexpr std::chrono::milliseconds NoTimeout{ -1 };

//////////////////////////////////////////////////////////////////////////
// Nothing else wakes the window for an auto refresh, so the wait ends at its deadline.
std::chrono::milliseconds BoundByDeadline(std::chrono::milliseconds wait, std::optional<std::chrono::steady_clock::time_point> deadline)
{
	std::chrono::milliseconds bounded{ wait };

	if (deadline.has_value())
	{
		std::chrono::milliseconds const untilDeadline{ std::max(std::chrono::ceil<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now()),
			std::chrono::milliseconds{ 0 }) };

		bounded = (wait == NoTimeout) ? untilDeadline : std::min(wait, untilDeadline);
	}

	return bounded;
}

//////////////////////////////////////////////////////////////////////////
std::string_view OrNone(char const* pText)
{
	return (pText != nullptr) ? std::string_view{ pText } : std::string_view{ "none" };
}

//////////////////////////////////////////////////////////////////////////
// The display scale includes the pixel density that ImGui's framebuffer scale already covers.
float ReadScale(SDL_Window* pWindow)
{
	float scale{ 1.0f };

	float const displayScale{ SDL_GetWindowDisplayScale(pWindow) };
	float const pixelDensity{ SDL_GetWindowPixelDensity(pWindow) };

	if (displayScale > 0.0f && pixelDensity > 0.0f)
	{
		scale = displayScale / pixelDensity;
	}
	else
	{
		gLog.Warning("Cannot read the window's display scale, using 1.0: {}", SDL_GetError());
	}

	return scale;
}

//////////////////////////////////////////////////////////////////////////
int ToPixels(uint32_t logical, float scale)
{
	return static_cast<int>(std::lround(static_cast<float>(logical) * scale));
}

//////////////////////////////////////////////////////////////////////////
// Clamped: below a scale of 1, rounding can fall just short of the minimum.
uint32_t ToLogical(int pixels, float scale, uint32_t minimum)
{
	uint32_t const logical{ static_cast<uint32_t>(std::max(std::lround(static_cast<float>(pixels) / scale), 0L)) };

	return std::clamp(logical, minimum, Config::MaxWindowSide);
}

//////////////////////////////////////////////////////////////////////////
void ReadLogicalSize(SDL_Window* pWindow, Config::SWindowSettings& window)
{
	int width{ 0 };
	int height{ 0 };

	if (SDL_GetWindowSize(pWindow, &width, &height))
	{
		float const scale{ ReadScale(pWindow) };

		window.width = ToLogical(width, scale, Config::MinWindowWidth);
		window.height = ToLogical(height, scale, Config::MinWindowHeight);
	}
	else
	{
		gLog.Warning("Cannot read the window size, keeping the last one known: {}", SDL_GetError());
	}
}

//////////////////////////////////////////////////////////////////////////
float ReadPrimaryDisplayScale()
{
	float scale{ SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()) };

	if (scale <= 0.0f)
	{
		gLog.Warning("Cannot read the primary display's scale, sizing the window for 1.0: {}", SDL_GetError());
		scale = 1.0f;
	}

	return scale;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CApplication::Initialize(SAboutInfo const& about, Config::SWindowSettings const& window, std::filesystem::path const& userDir)
{
	std::string const versionText{ about.version };

	m_about = about;
	m_userDir = userDir;

	if (!SDL_SetAppMetadata("Lookout", versionText.c_str(), "lookout"))
	{
		gLog.Warning("Cannot set the application metadata: {}", SDL_GetError());
	}

	bool initialized{ false };

	// Closing the window is asked of the main window, which may keep it open; SDL would quit behind its back.
	if (!SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0"))
	{
		gLog.Warning("Cannot keep SDL from quitting when the window closes: {}", SDL_GetError());
	}

	if (SDL_Init(SDL_INIT_VIDEO))
	{
		m_wakeEventType = SDL_RegisterEvents(1);

		if (m_wakeEventType == 0)
		{
			gLog.Error("Cannot register the event that wakes the window for query results: {}", SDL_GetError());
		}

		initialized = m_wakeEventType != 0 && CreateWindowAndRenderer(window) && InitializeImGui() && ShowMainWindow();
	}
	else
	{
		gLog.Error("Cannot initialize SDL video: {}", SDL_GetError());
	}

	return initialized;
}

//////////////////////////////////////////////////////////////////////////
// Waking is all it does: the event, like any other, opens a burst of frames in which the browser takes its results.
std::function<void()> CApplication::MakeWakeCallback() const
{
	TGE_ASSERT(m_wakeEventType != 0, "The wake callback is made before Initialize registered its event");

	return [eventType = m_wakeEventType]()
	{
		SDL_Event event{};

		event.type = eventType;

		if (!SDL_PushEvent(&event))
		{
			gLog.Warning("Cannot wake the window for query results: {}", SDL_GetError());
		}
	};
}

//////////////////////////////////////////////////////////////////////////
// From any thread; the reload itself waits for the end of a frame.
std::function<void()> CApplication::MakeReloadCallback()
{
	return [this, wake = MakeWakeCallback()]()
	{
		m_isReloadRequested.store(true);
		wake();
	};
}

//////////////////////////////////////////////////////////////////////////
void CApplication::Run(Browser::CBrowser& browser, std::function<void()> const& reloadGames)
{
	CMainWindow mainWindow{};
	auto lastUpdate{ std::chrono::steady_clock::now() };

	mainWindow.Initialize(m_pWindow, m_window.detailsWidth, m_about, m_userDir, MakeReloadCallback(), MakeWakeCallback());
	m_activeUntil = lastUpdate + ActiveDuration;

	while (Tge::gRuntime->CanRun())
	{
		SDL_Event event{};

		bool const isActive{ std::chrono::steady_clock::now() < m_activeUntil };
		std::chrono::milliseconds const frameWait{ m_hasVsync ? std::chrono::milliseconds{ 0 } : UnsyncedFrameInterval };
		// Neither file dialog wakes the loop: the portal answers over D-Bus, which SDL reads only while pumping events.
		std::chrono::milliseconds const idleWait{ BoundByDeadline(IsFileDialogPending() ? DialogPollInterval : NoTimeout, browser.GetNextAutoRefresh()) };
		std::chrono::milliseconds const wait{ isActive ? frameWait : idleWait };
		bool hasEvent{ SDL_WaitEventTimeout(&event, static_cast<Sint32>(wait.count())) };

		if (wait == NoTimeout && !hasEvent)
		{
			gLog.Error("Waiting for events failed, quitting: {}", SDL_GetError());
			Tge::gRuntime->Quit();
		}

		while (hasEvent)
		{
			ProcessEvent(event);
			hasEvent = SDL_PollEvent(&event);
		}

		if (std::exchange(m_isQuitRequested, false))
		{
			mainWindow.RequestQuit();
		}

		// No one sees the list of a minimised, hidden or suspended window; an overdue refresh runs once it shows again.
		browser.SetAutoRefreshPaused((SDL_GetWindowFlags(m_pWindow) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED)) != 0);
		browser.Update();

		auto const now{ std::chrono::steady_clock::now() };
		float const deltaTime{ std::chrono::duration<float>{ now - lastUpdate }.count() };

		lastUpdate = now;
		Tge::gRuntime->Update(deltaTime);

		if (Tge::gRuntime->CanRun())
		{
			bool const wasDialogPending{ IsFileDialogPending() };

			DrawFrame(browser, mainWindow);

			// Taking a dialog's answer changes the layout, which settles a frame later.
			if (wasDialogPending && !IsFileDialogPending())
			{
				m_activeUntil = std::chrono::steady_clock::now() + ActiveDuration;
			}

			if (CanReload() && m_isReloadRequested.exchange(false))
			{
				ReloadGames(mainWindow, reloadGames);
			}
		}
	}

	m_window.detailsWidth = mainWindow.GetDetailsWidth();
	mainWindow.Terminate();
}

//////////////////////////////////////////////////////////////////////////
// Read directly when normal: window managers report maximising and resizing in either order.
Config::SWindowSettings CApplication::GetWindowSettings() const
{
	TGE_ASSERT(m_pWindow != nullptr, "The window settings are read from a window that was never created");

	Config::SWindowSettings window{ m_window };

	window.layout = ImGui::SaveIniSettingsToMemory();

	if (!m_window.isMaximized && (SDL_GetWindowFlags(m_pWindow) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN)) == 0)
	{
		ReadLogicalSize(m_pWindow, window);
	}

	return window;
}

//////////////////////////////////////////////////////////////////////////
void CApplication::Terminate()
{
	if (m_isImGuiInitialized)
	{
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		m_isImGuiInitialized = false;
	}

	if (m_pRenderer != nullptr)
	{
		gGameIcons.Terminate();
		gFlagAtlas.Terminate();
		SDL_DestroyRenderer(m_pRenderer);
		m_pRenderer = nullptr;
	}

	if (m_pWindow != nullptr)
	{
		SDL_DestroyWindow(m_pWindow);
		m_pWindow = nullptr;
	}

	SDL_Quit();
}

//////////////////////////////////////////////////////////////////////////
bool CApplication::CreateWindowAndRenderer(Config::SWindowSettings const& window)
{
	float const initialScale{ ReadPrimaryDisplayScale() };
	int width{ ToPixels(window.width, initialScale) };
	int height{ ToPixels(window.height, initialScale) };
	SDL_Rect usableBounds{};

	if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usableBounds))
	{
		width = std::min(width, usableBounds.w);
		height = std::min(height, usableBounds.h);
	}
	else
	{
		gLog.Warning("Cannot read the display's usable area, so the saved window size is not fitted to it: {}", SDL_GetError());
	}

	SDL_WindowFlags flags{ SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN };

	if (window.isMaximized)
	{
		flags |= SDL_WINDOW_MAXIMIZED;
	}

	m_pWindow = SDL_CreateWindow("Lookout", width, height, flags);

	if (m_pWindow != nullptr)
	{
		m_window = window;
		m_pRenderer = SDL_CreateRenderer(m_pWindow, nullptr);

		if (m_pRenderer != nullptr)
		{
			m_hasVsync = SDL_SetRenderVSync(m_pRenderer, 1);

			if (!m_hasVsync)
			{
				gLog.Warning("Cannot enable vsync, frames are paced by the event timeout only: {}", SDL_GetError());
			}

			gFlagAtlas.Initialize(m_pRenderer);
			gGameIcons.Initialize(m_pRenderer);
		}
		else
		{
			gLog.Error("Cannot create a renderer: {}", SDL_GetError());
		}
	}
	else
	{
		gLog.Error("Cannot create the window: {}", SDL_GetError());
	}

	return m_pRenderer != nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CApplication::InitializeImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io{ ImGui::GetIO() };

	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::LoadIniSettingsFromMemory(m_window.layout.data(), m_window.layout.size());

	bool const hasFonts{ LoadFonts() };

	if (hasFonts && ImGui_ImplSDL3_InitForSDLRenderer(m_pWindow, m_pRenderer))
	{
		if (ImGui_ImplSDLRenderer3_Init(m_pRenderer))
		{
			m_isImGuiInitialized = true;
		}
		else
		{
			gLog.Error("Cannot initialize ImGui's SDL renderer backend");
			ImGui_ImplSDL3_Shutdown();
		}
	}
	else if (hasFonts)
	{
		gLog.Error("Cannot initialize ImGui's SDL platform backend");
	}

	if (!m_isImGuiInitialized)
	{
		ImGui::DestroyContext();
	}

	return m_isImGuiInitialized;
}

//////////////////////////////////////////////////////////////////////////
bool CApplication::ShowMainWindow()
{
	m_scale = ReadScale(m_pWindow);
	ApplyTheme(m_scale);
	ApplyMinimumSize();

	bool const isShown{ SDL_ShowWindow(m_pWindow) };

	if (isShown)
	{
		gLog.Info("Display: SDL {}.{}.{}, video driver '{}', renderer '{}', display scale {:.2f}, pixel density {:.2f}, UI scale {:.2f}",
			SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
			OrNone(SDL_GetCurrentVideoDriver()), OrNone(SDL_GetRendererName(m_pRenderer)),
			SDL_GetWindowDisplayScale(m_pWindow), SDL_GetWindowPixelDensity(m_pWindow), m_scale);
	}
	else
	{
		gLog.Error("Cannot show the window: {}", SDL_GetError());
	}

	return isShown;
}

//////////////////////////////////////////////////////////////////////////
void CApplication::ProcessEvent(SDL_Event const& event)
{
	ImGui_ImplSDL3_ProcessEvent(&event);
	m_activeUntil = std::chrono::steady_clock::now() + ActiveDuration;

	switch (event.type)
	{
		case SDL_EVENT_QUIT:
			Tge::gRuntime->Quit();
			break;

		// The main window asks first when the game editor holds unsaved changes.
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			m_isQuitRequested = m_isQuitRequested || event.window.windowID == SDL_GetWindowID(m_pWindow);
			break;

		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			UpdateScale();
			break;

		case SDL_EVENT_WINDOW_RESIZED:
			TrackNormalSize();
			break;

		// Not read from the flags at exit: minimising clears SDL's maximised flag.
		case SDL_EVENT_WINDOW_MAXIMIZED:
			m_window.isMaximized = true;
			break;

		case SDL_EVENT_WINDOW_RESTORED:
			m_window.isMaximized = false;
			break;

		default:
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CApplication::TrackNormalSize()
{
	if ((SDL_GetWindowFlags(m_pWindow) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN)) == 0)
	{
		ReadLogicalSize(m_pWindow, m_window);
	}
}

//////////////////////////////////////////////////////////////////////////
void CApplication::UpdateScale()
{
	float const scale{ ReadScale(m_pWindow) };

	if (std::abs(scale - m_scale) > ScaleTolerance)
	{
		m_scale = scale;
		ApplyTheme(m_scale);
		ApplyMinimumSize();
		gLog.Info("UI scale changed to {:.2f}", m_scale);
	}
}

//////////////////////////////////////////////////////////////////////////
// So the window can never become smaller than the settings file accepts.
void CApplication::ApplyMinimumSize()
{
	if (!SDL_SetWindowMinimumSize(m_pWindow, ToPixels(Config::MinWindowWidth, m_scale), ToPixels(Config::MinWindowHeight, m_scale)))
	{
		gLog.Warning("Cannot set the window's minimum size: {}", SDL_GetError());
	}
}

//////////////////////////////////////////////////////////////////////////
void CApplication::DrawFrame(Browser::CBrowser& browser, CMainWindow& mainWindow)
{
	if ((SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_MINIMIZED) == 0)
	{
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		DrawMainWindow(browser, mainWindow);

		ImGui::Render();

		ImGuiIO const& io{ ImGui::GetIO() };
		ImVec4 const background{ GetBackgroundColor() };

		bool rendered{ SDL_SetRenderScale(m_pRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y) };

		rendered = SDL_SetRenderDrawColorFloat(m_pRenderer, background.x, background.y, background.z, background.w) && rendered;
		rendered = SDL_RenderClear(m_pRenderer) && rendered;
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_pRenderer);
		rendered = SDL_RenderPresent(m_pRenderer) && rendered;

		// Once: a failure that recurs every frame would otherwise bury the log.
		if (!rendered && !m_hasReportedRenderFailure)
		{
			gLog.Error("Drawing a frame failed: {}", SDL_GetError());
			m_hasReportedRenderFailure = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// AddFontFromMemoryTTF takes a mutable pointer but only reads the data when the atlas does not own it.
bool CApplication::LoadFonts()
{
	ImGuiIO& io{ ImGui::GetIO() };
	ImFontConfig textConfig{};

	textConfig.FontDataOwnedByAtlas = false;

	bool const hasTextFont{ io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(Embedded::RobotoMedium.data()), static_cast<int>(Embedded::RobotoMedium.size()),
		BaseFontSize, &textConfig) != nullptr };

	if (hasTextFont)
	{
		ImFontConfig iconConfig{};

		iconConfig.FontDataOwnedByAtlas = false;
		iconConfig.MergeMode = true;
		iconConfig.PixelSnapH = true;
		iconConfig.GlyphMinAdvanceX = BaseFontSize;
		iconConfig.GlyphExcludeRanges = IconExcludedRanges.data();

		if (io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(Embedded::FontAwesomeSolid.data()), static_cast<int>(Embedded::FontAwesomeSolid.size()),
			BaseFontSize, &iconConfig) == nullptr)
		{
			gLog.Warning("Cannot load the embedded icon font, so icons show as missing glyphs");
		}
	}
	else
	{
		gLog.Error("Cannot load the embedded UI font");
	}

	return hasTextFont;
}

//////////////////////////////////////////////////////////////////////////
void CApplication::DrawMainWindow(Browser::CBrowser& browser, CMainWindow& mainWindow)
{
	ImGuiViewport const* const pViewport{ ImGui::GetMainViewport() };

	ImGui::SetNextWindowPos(pViewport->WorkPos);
	ImGui::SetNextWindowSize(pViewport->WorkSize);

	// Not NoSavedSettings: the tables inside would inherit it and forget their columns.
	constexpr ImGuiWindowFlags MainWindowFlags{ ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar };

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	bool const isOpen{ ImGui::Begin("Lookout", nullptr, MainWindowFlags) };

	ImGui::PopStyleVar();

	if (isOpen)
	{
		mainWindow.Draw(browser);
	}

	ImGui::End();
}
//////////////////////////////////////////////////////////////////////////
// A popup, a file dialog's answer or a dragged card holds a game by its number, which a reload changes.
bool CApplication::CanReload() const
{
	return !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && !IsFileDialogPending()
		&& ImGui::GetDragDropPayload() == nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CApplication::ReloadGames(CMainWindow& mainWindow, std::function<void()> const& reloadGames)
{
	std::vector<std::string> oldKeys{};

	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		oldKeys.emplace_back(game.key);
	}

	gGameIcons.Terminate();
	reloadGames();
	gGameIcons.Initialize(m_pRenderer);
	mainWindow.OnCatalogChanged(oldKeys);
	m_activeUntil = std::chrono::steady_clock::now() + ActiveDuration;
}
} // namespace Lkt::Ui
