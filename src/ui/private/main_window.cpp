#include "main_window.hpp"
#include "frame_intents.hpp"
#include "game_sidebar.hpp"
#include "join_message.hpp"
#include "loggers.hpp"
#include "server_details.hpp"
#include "server_table.hpp"
#include "status_bar.hpp"
#include "theme.hpp"
#include "browser/browser.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <tge/module/runtime.hpp>
#include <imgui.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>

namespace Lkt::Ui
{
namespace
{
constexpr float SidebarEm{ 20.0f };
constexpr float MinSidebarEm{ 9.0f };
constexpr float MinTableEm{ 28.0f };
constexpr float MinDetailsEm{ 14.0f };
constexpr float MaxDetailsEm{ 40.0f };

//////////////////////////////////////////////////////////////////////////
void DrawMenuBar(SFrameIntents& intents)
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

		ImGui::EndMenuBar();
	}
}

//////////////////////////////////////////////////////////////////////////
// Shortcuts register every frame; under an open prompt, the ones that would act behind it are ignored.
void ReadShortcuts(SFrameIntents& intents)
{
	bool const isPopupOpen{ ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) };
	bool const wantsQuit{ ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q, ImGuiInputFlags_RouteGlobal) };
	bool const wantsRefresh{ ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal) };
	bool const wantsAddServer{ ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal) };

	intents.quit = wantsQuit || intents.quit;
	intents.refresh = (wantsRefresh && !isPopupOpen) || intents.refresh;
	intents.openAddServer = (wantsAddServer && !isPopupOpen) || intents.openAddServer;
}

//////////////////////////////////////////////////////////////////////////
std::string GetDisplayName(Browser::SServerEntry const& entry)
{
	return entry.summary.name.plain.empty() ? Query::FormatAddress(entry.address) : entry.summary.name.plain;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CMainWindow::Initialize(SDL_Window* pWindow, uint32_t detailsWidth, SAboutInfo const& about)
{
	m_gameSettings.Initialize(pWindow);
	m_aboutDialog.Initialize(pWindow, about);
	m_detailsEm = static_cast<float>(detailsWidth) / BaseFontSize;
}

//////////////////////////////////////////////////////////////////////////
void CMainWindow::Draw(Browser::CBrowser& browser)
{
	SFrameIntents intents{};

	DrawMenuBar(intents);
	ReadShortcuts(intents);
	m_toolbar.Draw(browser, intents);
	DrawBody(browser, intents);
	DrawStatusBar(browser, m_message);

	if (m_isNarrow)
	{
		DrawDetailsToggle();
	}

	Apply(browser, intents);
	m_gameSettings.Draw(browser, m_message);

	std::optional<Query::SServerAddress> const added{ m_addServerPrompt.Draw(browser) };

	if (added.has_value())
	{
		GetSelectedKey(browser.GetSelectedGame()) = Query::ToKey(*added);
		m_shouldScrollToSelection = true;
		m_message = std::format("Added {} to the favourites", Query::FormatAddress(*added));
	}

	m_passwordPrompt.Draw(browser, m_message);
	m_aboutDialog.Draw();
}

//////////////////////////////////////////////////////////////////////////
uint32_t CMainWindow::GetDetailsWidth() const
{
	return static_cast<uint32_t>(std::lround(m_detailsEm * BaseFontSize));
}

//////////////////////////////////////////////////////////////////////////
// ImGui resizes a child only from its right edge, so the details pane on the right gets a splitter of its own.
void CMainWindow::DrawBody(Browser::CBrowser const& browser, SFrameIntents& intents)
{
	float const em{ ImGui::GetFontSize() };
	float const statusHeight{ ImGui::GetFrameHeightWithSpacing() };
	uint64_t& selectedKey{ GetSelectedKey(browser.GetSelectedGame()) };

	if (ImGui::BeginChild("##body", ImVec2{ 0.0f, -statusHeight }))
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2{ MinSidebarEm * em, 0.0f }, ImVec2{ FLT_MAX, FLT_MAX });
		ImGui::BeginChild("##games", ImVec2{ SidebarEm * em, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
		DrawGameSidebar(browser, intents);
		ImGui::EndChild();
		ImGui::SameLine();

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

	ImGui::EndChild();
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

	if (intents.filter.has_value())
	{
		browser.SetFilter(*intents.filter);
	}

	if (intents.sort.has_value())
	{
		browser.SetSort(*intents.sort);
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

	if (intents.openGameSettings.has_value())
	{
		m_gameSettings.Open(*intents.openGameSettings);
	}

	if (intents.quit)
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
				CopyAddress(address);
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
		m_passwordPrompt.Open(Query::ToKey(entry.address), GetDisplayName(entry), std::string{ launcherId });
	}
	else
	{
		m_message = DescribeJoin(Query::GetGame(game).name, entry.address, launcher, browser.Join(entry.address, {}, launcherId));
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
