#include "game_settings_popup.hpp"
#include "file_dialog.hpp"
#include "icons.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "browser/browser.hpp"
#include "config/install_ids.hpp"
#include "launch/folder_option.hpp"
#include "launch/quote_argument.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/utf8.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <format>
#include <span>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "###game-settings" };
constexpr float FieldEm{ 26.0f };

//////////////////////////////////////////////////////////////////////////
void DrawOptionTooltip(Launch::SLaunchOption const& option)
{
	std::string_view const program{ option.argv.empty() ? std::string_view{} : std::string_view{ option.argv.front() } };

	ImGui::SetItemTooltip("%.*s", static_cast<int>(program.size()), program.data());
}

//////////////////////////////////////////////////////////////////////////
void DrawError(std::string_view text, float width)
{
	ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().error);
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
}

//////////////////////////////////////////////////////////////////////////
float GetButtonWidth(std::string_view label)
{
	return ImGui::CalcTextSize(label.data(), label.data() + label.size()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

//////////////////////////////////////////////////////////////////////////
// A popup is placed only when it appears, so one that grows could push its end off screen.
void KeepWindowInside(ImGuiViewport const& viewport)
{
	ImVec2 const position{ ImGui::GetWindowPos() };
	ImVec2 const size{ ImGui::GetWindowSize() };
	float const maxX{ std::max(viewport.WorkPos.x, viewport.WorkPos.x + viewport.WorkSize.x - size.x) };
	float const maxY{ std::max(viewport.WorkPos.y, viewport.WorkPos.y + viewport.WorkSize.y - size.y) };
	ImVec2 const inside{ std::clamp(position.x, viewport.WorkPos.x, maxX), std::clamp(position.y, viewport.WorkPos.y, maxY) };

	if (inside.x != position.x || inside.y != position.y)
	{
		ImGui::SetWindowPos(inside);
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::Initialize(SDL_Window* pWindow)
{
	m_pWindow = pWindow;
}

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::Open(Query::EGame game)
{
	m_game = game;
	m_shouldOpen = true;
}

//////////////////////////////////////////////////////////////////////////
// Opened at the mouse, so beside the gear that was clicked.
void CGameSettingsPopup::Draw(Browser::CBrowser& browser, std::string& message)
{
	ImGuiViewport const* const pViewport{ ImGui::GetMainViewport() };

	TakeDialogResult(browser, message);

	if (m_shouldOpen)
	{
		m_edits.clear();
		ImGui::OpenPopup(PopupId);
		m_shouldOpen = false;
	}

	ImGui::SetNextWindowSizeConstraints(ImVec2{ 0.0f, 0.0f }, pViewport->WorkSize);

	if (ImGui::BeginPopup(PopupId))
	{
		SyncEdits(browser);
		DrawFound(browser);
		DrawInstalls(browser);
		KeepWindowInside(*pViewport);
		ImGui::EndPopup();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::TakeDialogResult(Browser::CBrowser& browser, std::string& message)
{
	std::optional<SFileDialogResult> const result{ TakeFileDialogResult() };

	if (result.has_value() && result->state == EFileDialogState::Picked)
	{
		// The settings writer would replace the bytes, and the saved path would lead nowhere.
		if (Query::IsValidUtf8(result->text))
		{
			UsePickedPath(browser, result->text);
		}
		else
		{
			gLog.Warning("'{}' cannot start {}: its path is not valid UTF-8", result->text, Query::GetGame(m_dialogGame).name);
			message = "The chosen path is not valid UTF-8, so Lookout cannot keep it";
		}
	}
	else if (result.has_value() && result->state == EFileDialogState::Failed)
	{
		gLog.Warning("The file dialog failed: {}", result->text);
		message = std::format("The file dialog failed: {}", result->text);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::UsePickedPath(Browser::CBrowser& browser, std::string_view path)
{
	if (m_dialogPurpose == EFileDialogPurpose::AddFolder)
	{
		browser.AddInstall(m_dialogGame, Config::EInstallKind::Folder, path);
	}
	else if (m_dialogPurpose == EFileDialogPurpose::ChangeFolder)
	{
		browser.SetInstallFolder(m_dialogGame, m_dialogInstallId, path);
	}
	else
	{
		std::string const command{ Launch::QuoteArgument(path) };
		auto const edit{ std::ranges::find(m_edits, m_dialogInstallId, &SInstallEdit::id) };

		browser.SetInstallCommand(m_dialogGame, m_dialogInstallId, command);

		if (m_dialogGame == m_game && edit != m_edits.end())
		{
			edit->command = command;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::SyncEdits(Browser::CBrowser const& browser)
{
	std::span<Config::SGameInstall const> const installs{ browser.GetSettings().games[static_cast<size_t>(m_game)].installs };
	bool const isInSync{ std::ranges::equal(m_edits, installs, {}, &SInstallEdit::id, &Config::SGameInstall::id) };

	if (!isInSync)
	{
		m_edits.clear();

		for (Config::SGameInstall const& install : installs)
		{
			m_edits.emplace_back(install.id, install.name, (install.kind == Config::EInstallKind::Command) ? install.location : std::string{});
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::DrawFound(Browser::CBrowser const& browser) const
{
	std::span<Launch::SLaunchOption const> const options{ browser.GetLaunchOptions(m_game) };

	ImGui::SeparatorText("Found");

	for (Launch::SLaunchOption const& option : options)
	{
		DrawLauncherLabel(option.name, option.location);
		DrawOptionTooltip(option);
	}

	if (options.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDisabled);
		ImGui::TextUnformatted("None found");
		ImGui::PopStyleColor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::DrawInstalls(Browser::CBrowser& browser)
{
	std::span<Config::SGameInstall const> const installs{ browser.GetSettings().games[static_cast<size_t>(m_game)].installs };
	std::span<Browser::SInstallLauncher const> const launchers{ browser.GetInstallLaunchers(m_game) };
	bool const hasCommand{ std::ranges::contains(installs, Config::EInstallKind::Command, &Config::SGameInstall::kind) };
	bool const isDialogPending{ IsFileDialogPending() };
	bool const canAddFolder{ Launch::IsFolderInstallSupported(Query::GetGame(m_game)) };
	std::optional<uint32_t> removedId{};

	ImGui::Spacing();
	ImGui::SeparatorText("Your installs");

	for (size_t index{ 0 }; index < installs.size(); ++index)
	{
		std::optional<uint32_t> const removed{ DrawInstall(browser, m_edits[index], installs[index], launchers[index]) };

		removedId = removed.has_value() ? removed : removedId;
	}

	ImGui::BeginDisabled(isDialogPending || !canAddFolder);

	if (ImGui::Button(LKT_ICON_PLUS " Folder…"))
	{
		OpenDialog(EFileDialogPurpose::AddFolder, 0, {});
	}

	ImGui::EndDisabled();

	if (!canAddFolder)
	{
		ImGui::SetItemTooltip("%.*s", static_cast<int>(Launch::ToString(Launch::ELaunchError::FolderInstallUnsupported).size()),
			Launch::ToString(Launch::ELaunchError::FolderInstallUnsupported).data());
	}

	ImGui::SameLine();

	if (ImGui::Button(LKT_ICON_PLUS " Command"))
	{
		browser.AddInstall(m_game, Config::EInstallKind::Command, {});
	}

	if (hasCommand)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDisabled);
		ImGui::TextUnformatted("Lookout appends +connect <address> to a command");
		ImGui::PopStyleColor();
	}

	if (removedId.has_value())
	{
		browser.RemoveInstall(m_game, *removedId);
	}
}

//////////////////////////////////////////////////////////////////////////
// Disabled while a dialog is pending: an active field writes its own text back every frame, over a pick.
std::optional<uint32_t> CGameSettingsPopup::DrawInstall(Browser::CBrowser& browser, SInstallEdit& edit, Config::SGameInstall const& install,
	Browser::SInstallLauncher const& launcher)
{
	ImGuiStyle const& style{ ImGui::GetStyle() };
	float const width{ FieldEm * ImGui::GetFontSize() };
	float const lineHeight{ ImGui::GetTextLineHeight() };
	bool const isFolder{ install.kind == Config::EInstallKind::Folder };
	std::string_view const buttonLabel{ isFolder ? std::string_view{ "Change…" } : std::string_view{ "Program…" } };
	float const fieldWidth{ width - GetButtonWidth(buttonLabel) - style.ItemSpacing.x };
	std::string_view const gameName{ Query::GetGame(m_game).name };
	std::optional<uint32_t> removedId{};

	ImGui::PushID(static_cast<int>(install.id));
	ImGui::BeginDisabled(IsFileDialogPending());
	ImGui::SetNextItemWidth(width - lineHeight - style.ItemSpacing.x);

	if (ImGui::InputTextWithHint("##name", gameName.data(), &edit.name))
	{
		browser.SetInstallName(m_game, install.id, edit.name);
	}

	ImGui::SameLine();

	if (IconButton("##remove", LKT_ICON_TRASH))
	{
		removedId = install.id;
	}

	ImGui::SetItemTooltip("Remove this install");

	if (isFolder)
	{
		ImVec2 const start{ ImGui::GetCursorScreenPos() };

		ImGui::Dummy(ImVec2{ fieldWidth, ImGui::GetFrameHeight() });
		DrawEllipsised(launcher.location, ImVec2{ start.x, start.y + style.FramePadding.y }, start.x + fieldWidth, GetThemeColors().textDisabled);
		ImGui::SetItemTooltip("%.*s", static_cast<int>(install.location.size()), install.location.data());
	}
	else
	{
		ImGui::SetNextItemWidth(fieldWidth);

		if (ImGui::InputTextWithHint("##command", "steam -applaunch 38430", &edit.command))
		{
			browser.SetInstallCommand(m_game, install.id, edit.command);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button(buttonLabel.data()))
	{
		OpenDialog(isFolder ? EFileDialogPurpose::ChangeFolder : EFileDialogPurpose::PickProgram, install.id, isFolder ? install.location : std::string{});
	}

	ImGui::EndDisabled();

	if (!launcher.option.has_value())
	{
		DrawError(Launch::ToString(launcher.option.error()), width);
	}

	ImGui::Spacing();
	ImGui::PopID();

	return removedId;
}

//////////////////////////////////////////////////////////////////////////
void CGameSettingsPopup::OpenDialog(EFileDialogPurpose purpose, uint32_t installId, std::string const& startFolder)
{
	m_dialogGame = m_game;
	m_dialogInstallId = installId;
	m_dialogPurpose = purpose;

	if (purpose == EFileDialogPurpose::PickProgram)
	{
		OpenProgramDialog(m_pWindow);
	}
	else
	{
		OpenFolderDialog(m_pWindow, startFolder.empty() ? nullptr : startFolder.c_str());
	}
}
} // namespace Lkt::Ui
