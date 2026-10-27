#include "main_window.hpp"
#include "format_to.hpp"
#include "frame_intents.hpp"
#include "game_sidebar.hpp"
#include "icons.hpp"
#include "join_message.hpp"
#include "loggers.hpp"
#include "server_details.hpp"
#include "server_table.hpp"
#include "status_bar.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "browser/browser.hpp"
#include "download/lookout_games.hpp"
#include "download/lookout_release.hpp"
#include "net/clock.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <tge/module/runtime.hpp>
#include <imgui.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Lkt::Ui
{
namespace
{
constexpr float SidebarEm{ 20.0f };
constexpr float MinSidebarEm{ 13.0f };
constexpr float MinTableEm{ 28.0f };
constexpr float MinDetailsEm{ 14.0f };
constexpr float MaxDetailsEm{ 40.0f };

//////////////////////////////////////////////////////////////////////////
std::string_view DescribeLatestRelease(Download::CLatestRelease const& latestRelease, std::string_view version, std::array<char, 256>& buffer)
{
	std::string_view const latest{ latestRelease.GetVersion() };
	std::string_view const problem{ latestRelease.GetProblem() };
	std::string_view description{};

	if (latest.empty() && problem.empty())
	{
		description = FormatTo(buffer, "Not known yet whether Lookout {} is the latest version", version);
	}
	else if (latest.empty())
	{
		description = FormatTo(buffer, "Cannot tell whether Lookout {} is the latest version: {}", version, problem);
	}
	else if (Download::IsNewerLookout(latest, version))
	{
		description = FormatTo(buffer, "Lookout {} is out; you have {}. Click to get it.", latest, version);
	}
	else if (Download::IsNewerLookout(version, latest))
	{
		description = FormatTo(buffer, "Lookout {} is newer than the latest release, {}", version, latest);
	}
	else
	{
		description = FormatTo(buffer, "Lookout {} is the latest version", version);
	}

	return description;
}

//////////////////////////////////////////////////////////////////////////
// At the menu bar's right end; only a newer release stands out, and only it can be clicked.
void DrawLatestRelease(Download::CLatestRelease const& latestRelease, std::string_view version)
{
	SThemeColors const& colors{ GetThemeColors() };
	std::string_view const latest{ latestRelease.GetVersion() };
	bool const isNewerOut{ Download::IsNewerLookout(latest, version) };
	float const spacing{ ImGui::GetStyle().ItemSpacing.x };
	std::array<char, 32> labelBuffer{};
	std::array<char, 256> tooltipBuffer{};
	std::string_view const label{ isNewerOut ? FormatTo(labelBuffer, LKT_ICON_CIRCLE_UP " {}", latest)
		: FormatTo(labelBuffer, "{}", latest.empty() ? LKT_ICON_QUESTION : LKT_ICON_CIRCLE_CHECK) };
	std::string_view const tooltip{ DescribeLatestRelease(latestRelease, version, tooltipBuffer) };
	float const width{ ImGui::CalcTextSize(label.data(), label.data() + label.size()).x + spacing };

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));

	if (isNewerOut)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, colors.amber);

		bool const isClicked{ ImGui::MenuItem(label.data()) };

		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());

		if (isClicked && !SDL_OpenURL(Download::LookoutReleaseUrl.data()))
		{
			gLog.Warning("Cannot open {}: {}", Download::LookoutReleaseUrl, SDL_GetError());
		}
	}
	else
	{
		// Where a menu item's text would start, so the icon does not move when it becomes one.
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing * 0.5f);
		ImGui::PushStyleColor(ImGuiCol_Text, colors.textDisabled);
		ImGui::TextUnformatted(label.data(), label.data() + label.size());
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawMenuBar(Download::CLatestRelease const& latestRelease, std::string_view version, SFrameIntents& intents)
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Lookout"))
		{
			intents.openAbout = ImGui::MenuItem("About Lookout") || intents.openAbout;
			ImGui::Separator();
			intents.quit = ImGui::MenuItem("Quit", "Ctrl+Q") || intents.quit;
			ImGui::EndMenu();
		}

		DrawLatestRelease(latestRelease, version);
		ImGui::EndMenuBar();
	}
}

//////////////////////////////////////////////////////////////////////////
// Shortcuts register every frame; under an open prompt, the ones that would act behind it are ignored.
void ReadShortcuts(bool hasGame, SFrameIntents& intents)
{
	bool const isPopupOpen{ ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) };
	bool const wantsQuit{ ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q, ImGuiInputFlags_RouteGlobal) };
	bool const wantsRefresh{ ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal) };
	bool const wantsAddServer{ ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal) };

	intents.quit = wantsQuit || intents.quit;
	intents.refresh = (wantsRefresh && !isPopupOpen) || intents.refresh;
	intents.openAddServer = (wantsAddServer && !isPopupOpen && hasGame) || intents.openAddServer;
}

//////////////////////////////////////////////////////////////////////////
void DrawNoGames(bool canDownload, SFrameIntents& intents)
{
	ImGui::BeginChild("##no-games", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Borders);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted("No games are installed yet.");
	ImGui::Spacing();
	ImGui::BeginDisabled(!canDownload);
	intents.openDownloads = ImGui::Button("Download games…") || intents.openDownloads;
	ImGui::EndDisabled();
	ImGui::Spacing();
	ImGui::TextDisabled("Or add one of your own with the gamepad button above.");
	ImGui::PopTextWrapPos();
	ImGui::EndChild();
}

//////////////////////////////////////////////////////////////////////////
std::string GetDisplayName(Browser::SServerEntry const& entry)
{
	return entry.summary.name.plain.empty() ? Query::FormatAddress(entry.joinAddress) : entry.summary.name.plain;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CMainWindow::Initialize(SDL_Window* pWindow, uint32_t detailsWidth, SAboutInfo const& about, std::filesystem::path const& userDir,
	std::filesystem::path const& cacheDir, std::function<void()> requestReload, std::function<void()> wake)
{
	m_userDir = userDir;
	m_version = about.version;
	m_requestReload = std::move(requestReload);
	m_masterProbes.Initialize(wake);

	if (!m_latestRelease.Initialize(Download::GetLookoutReleaseOrigin(about.version), wake))
	{
		gLog.Warning("Lookout cannot ask whether a newer version is out: the check could not be set up");
	}

	m_canDownload = !userDir.empty() && m_downloads.Initialize(userDir, cacheDir, Download::GetLookoutGamesSource(about.version), std::move(wake));

	if (!userDir.empty() && !m_canDownload)
	{
		gLog.Warning("Games cannot be downloaded: the download could not be set up");
	}

	m_downloadWindow.Initialize(SDL_GetRenderer(pWindow), m_canDownload ? &m_downloads : nullptr, &m_latestRelease, about.version);
	m_gameSettings.Initialize(pWindow);
	m_aboutDialog.Initialize(pWindow, about);
	m_detailsEm = static_cast<float>(detailsWidth) / BaseFontSize;
	m_selectedKeys.assign(Query::GetGameCatalog().size(), NoSelection);
}

//////////////////////////////////////////////////////////////////////////
// The catalog numbers its games anew, so what is kept per game follows its key.
void CMainWindow::OnCatalogChanged(std::span<std::string const> oldKeys)
{
	std::vector<uint64_t> selectedKeys(Query::GetGameCatalog().size(), NoSelection);

	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		auto const old{ std::ranges::find(oldKeys, game.key) };

		if (old != oldKeys.end())
		{
			selectedKeys[static_cast<size_t>(game.game)] = m_selectedKeys[static_cast<size_t>(old - oldKeys.begin())];
		}
	}

	m_selectedKeys = std::move(selectedKeys);
	m_toolbar.OnCatalogChanged();
	m_gameEditor.OnCatalogChanged(m_userDir);
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::Terminate()
{
	m_downloadWindow.Terminate();
	m_downloads.Terminate();
	m_canDownload = false;
	m_latestRelease.Terminate();
	m_masterProbes.Terminate();
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::RequestQuit()
{
	m_isQuitRequested = true;
}

//////////////////////////////////////////////////////////////////////////
// A first start with no games opens the download window by itself.
void CMainWindow::Draw(Browser::CBrowser& browser)
{
	SFrameIntents intents{};
	bool const hasNewDownloads{ m_canDownload && m_downloads.Update() };

	m_masterProbes.Update();
	m_latestRelease.Update(Net::Clock::now());

	intents.quit = std::exchange(m_isQuitRequested, false);

	if (!m_hasStarted)
	{
		intents.openDownloads = Query::GetGameCatalog().empty();
		m_hasStarted = true;
	}

	DrawMenuBar(m_latestRelease, m_version, intents);
	ReadShortcuts(browser.GetSelectedGame() != Query::NoGame, intents);
	m_toolbar.Draw(browser, !m_userDir.empty(), m_downloadWindow.CanOpen(), m_serverPaneX, intents);
	DrawBody(browser, intents);
	DrawStatusBar(browser, m_message);

	if (m_isNarrow)
	{
		DrawDetailsToggle();
	}

	Apply(browser, intents);
	m_gameSettings.Draw(browser, m_message);

	std::optional<Query::SServerAddress> const added{ m_addServerPrompt.Draw(browser) };

	Browser::SServerEntry const* const pAdded{ added.has_value() ? browser.FindEntry(Query::ToKey(*added)) : nullptr };

	if (pAdded != nullptr)
	{
		GetSelectedKey(browser.GetSelectedGame()) = Query::ToKey(*added);
		m_shouldScrollToSelection = true;
		m_message = std::format("Added {} to the favourites", Query::FormatAddress(pAdded->joinAddress));
	}

	m_passwordPrompt.Draw(browser, m_message);
	m_aboutDialog.Draw();
	DrawGamePrompts();
	m_downloadWindow.Draw();

	if (hasNewDownloads)
	{
		m_requestReload();
	}
}

//////////////////////////////////////////////////////////////////////////
uint32_t CMainWindow::GetDetailsWidth() const
{
	return static_cast<uint32_t>(std::lround(m_detailsEm * BaseFontSize));
}

//////////////////////////////////////////////////////////////////////////
// What only time brings, with no event to wake the window for it.
std::optional<std::chrono::steady_clock::time_point> CMainWindow::GetNextDeadline() const
{
	std::optional<std::chrono::steady_clock::time_point> deadline{ m_masterProbes.GetNextDeadline() };
	std::optional<std::chrono::steady_clock::time_point> const check{ m_latestRelease.GetNextCheck() };

	if (check.has_value() && (!deadline.has_value() || *check < *deadline))
	{
		deadline = check;
	}

	return deadline;
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::DrawBody(Browser::CBrowser const& browser, SFrameIntents& intents)
{
	float const em{ ImGui::GetFontSize() };
	float const statusHeight{ ImGui::GetFrameHeightWithSpacing() };

	if (ImGui::BeginChild("##body", ImVec2{ 0.0f, -statusHeight }))
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2{ MinSidebarEm * em, 0.0f }, ImVec2{ FLT_MAX, FLT_MAX });
		ImGui::BeginChild("##games", ImVec2{ SidebarEm * em, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
		DrawGameSidebar(browser, m_userDir, intents);
		ImGui::EndChild();
		ImGui::SameLine();
		m_serverPaneX = ImGui::GetCursorScreenPos().x;

		if (browser.GetSelectedGame() != Query::NoGame)
		{
			DrawServerPanes(browser, intents);
		}
		else
		{
			DrawNoGames(m_downloadWindow.CanOpen(), intents);
			m_isNarrow = false;
		}
	}

	ImGui::EndChild();
}

//////////////////////////////////////////////////////////////////////////
// ImGui resizes a child only from its right edge, so the details pane on the right gets a splitter of its own.
void CMainWindow::DrawServerPanes(Browser::CBrowser const& browser, SFrameIntents& intents)
{
	float const em{ ImGui::GetFontSize() };
	uint64_t& selectedKey{ GetSelectedKey(browser.GetSelectedGame()) };

	float const splitterWidth{ ImGui::GetStyle().ItemSpacing.x };
	float const available{ ImGui::GetContentRegionAvail().x };
	float const maxDetailsEm{ std::min(MaxDetailsEm, (available - splitterWidth) / em - MinTableEm) };
	float const detailsEm{ std::min(std::clamp(m_detailsEm, MinDetailsEm, MaxDetailsEm), maxDetailsEm) };
	bool const isSideBySide{ detailsEm >= MinDetailsEm };

	if (isSideBySide)
	{
		ImGui::BeginChild("##servers", ImVec2{ available - detailsEm * em - splitterWidth, 0.0f });
		DrawServerTable(browser, selectedKey, m_shouldScrollToSelection, intents);
		ImGui::EndChild();
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::InvisibleButton("##splitter", ImVec2{ splitterWidth, std::max(1.0f, ImGui::GetContentRegionAvail().y) });

		if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}

		if (ImGui::IsItemActive())
		{
			m_detailsEm = std::clamp(detailsEm - ImGui::GetIO().MouseDelta.x / em, MinDetailsEm, maxDetailsEm);
		}

		ImGui::SameLine(0.0f, 0.0f);
		ImGui::BeginChild("##details", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Borders);
		DrawServerDetails(browser, selectedKey, intents);
		ImGui::EndChild();
	}
	else if (m_isDetailsShown)
	{
		ImGui::BeginChild("##details", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Borders);
		DrawServerDetails(browser, selectedKey, intents);
		ImGui::EndChild();
	}
	else
	{
		ImGui::BeginChild("##servers", ImVec2{ 0.0f, 0.0f });
		DrawServerTable(browser, selectedKey, m_shouldScrollToSelection, intents);
		ImGui::EndChild();
	}

	m_isNarrow = !isSideBySide;
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::DrawDetailsToggle()
{
	char const* const label{ m_isDetailsShown ? "Servers###details-toggle" : "Details###details-toggle" };
	float const width{ ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f };

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));

	if (ImGui::SmallButton(label))
	{
		m_isDetailsShown = !m_isDetailsShown;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::Apply(Browser::CBrowser& browser, SFrameIntents const& intents)
{
	if (intents.selectGame.has_value())
	{
		browser.SelectGame(*intents.selectGame);
		m_shouldScrollToSelection = true;
		m_message.clear();
	}

	if (intents.hideGame.has_value())
	{
		SetGameListed(browser, *intents.hideGame, false);
	}

	if (intents.showGame.has_value())
	{
		SetGameListed(browser, *intents.showGame, true);
	}

	if (intents.moveGame.has_value())
	{
		browser.MoveGame(intents.moveGame->game, intents.moveGame->target);
	}

	if (intents.filter.has_value())
	{
		browser.SetFilter(*intents.filter);
	}

	if (intents.sort.has_value())
	{
		browser.SetSort(*intents.sort);
	}

	if (intents.autoRefreshSeconds.has_value())
	{
		browser.SetAutoRefresh(*intents.autoRefreshSeconds);
	}

	if (intents.refresh)
	{
		browser.Refresh();
	}

	if (intents.action.action != EServerAction::None)
	{
		Handle(browser, intents.action);
	}

	if (intents.openAddServer)
	{
		m_addServerPrompt.Open();
	}

	if (intents.openAbout)
	{
		m_aboutDialog.Open();
	}

	if (intents.openDownloads)
	{
		m_downloadWindow.Open();
	}

	if (intents.openGameSettings.has_value())
	{
		m_gameSettings.Open(*intents.openGameSettings);
	}

	if (intents.openAddGame)
	{
		m_gameEditor.OpenNew();
	}

	if (intents.editGame.has_value())
	{
		m_gameEditor.Open(*intents.editGame, m_userDir);
	}

	if (intents.quit && m_gameEditor.HasUnsavedChanges())
	{
		m_gameEditor.AskToQuit();
	}
	else if (intents.quit)
	{
		Tge::gRuntime->Quit();
	}
}

//////////////////////////////////////////////////////////////////////////
// Hiding the selected game selects another, which is then treated as any game switch.
void CMainWindow::SetGameListed(Browser::CBrowser& browser, Query::EGame game, bool isListed)
{
	Query::EGame const selected{ browser.GetSelectedGame() };

	browser.SetGameListed(game, isListed);

	if (browser.GetSelectedGame() != selected)
	{
		m_shouldScrollToSelection = true;
		m_message.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
// What they change in the data folder is read again at the end of a frame with no popup open.
void CMainWindow::DrawGamePrompts()
{
	EEditorOutcome const edited{ m_gameEditor.Draw(m_userDir, m_message, m_masterProbes) };

	if (edited == EEditorOutcome::FilesChanged)
	{
		m_requestReload();
	}

	if (edited == EEditorOutcome::Quit)
	{
		Tge::gRuntime->Quit();
	}
}

//////////////////////////////////////////////////////////////////////////
// Looked up again: a game switch applied earlier in the same frame leaves no such server.
void CMainWindow::Handle(Browser::CBrowser& browser, SServerAction const& action)
{
	Browser::SServerEntry const* const pEntry{ browser.FindEntry(action.key) };

	if (pEntry != nullptr)
	{
		Query::SServerAddress const address{ pEntry->address };

		switch (action.action)
		{
			case EServerAction::Join:
				Join(browser, *pEntry, action.launcherId);
				break;
			case EServerAction::Refresh:
				browser.RefreshServer(address);
				break;
			case EServerAction::ToggleFavourite:
				browser.ToggleFavourite(address);
				break;
			case EServerAction::CopyAddress:
				CopyAddress(pEntry->joinAddress);
				break;
			case EServerAction::None:
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// A missing launcher is reported at once, before anyone types a password for nothing.
void CMainWindow::Join(Browser::CBrowser& browser, Browser::SServerEntry const& entry, std::string_view launcherId)
{
	Query::EGame const game{ browser.GetSelectedGame() };
	std::expected<Launch::SLaunchOption, Launch::ELaunchError> const launcher{ browser.ResolveLauncher(game, launcherId) };

	if (launcher.has_value() && entry.summary.hasPassword)
	{
		m_passwordPrompt.Open(entry.joinAddress, GetDisplayName(entry), std::string{ launcherId });
	}
	else
	{
		m_message = DescribeJoin(Query::GetGame(game).name, entry.joinAddress, launcher, browser.Join(entry.joinAddress, {}, launcherId));
	}
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::CopyAddress(Query::SServerAddress const& address)
{
	std::string const text{ Query::FormatAddress(address) };

	if (SDL_SetClipboardText(text.c_str()))
	{
		m_message = std::format("Copied {}", text);
	}
	else
	{
		gLog.Warning("Cannot copy {} to the clipboard: {}", text, SDL_GetError());
		m_message = std::format("Cannot copy {} to the clipboard", text);
	}
}

//////////////////////////////////////////////////////////////////////////
uint64_t& CMainWindow::GetSelectedKey(Query::EGame game)
{
	return m_selectedKeys[static_cast<size_t>(game)];
}
} // namespace Lkt::Ui
