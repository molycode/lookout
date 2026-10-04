#include "game_editor.hpp"
#include "format_to.hpp"
#include "game_reference.hpp"
#include "icons.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "games/check_game.hpp"
#include "games/game_files.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <array>
#include <expected>
#include <format>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "###game-editor" };
constexpr float SizeFraction{ 0.8f };
constexpr float TextFraction{ 0.6f };
constexpr float MinWidthEm{ 40.0f };
constexpr float MinHeightEm{ 20.0f };
} // namespace

//////////////////////////////////////////////////////////////////////////
void CGameEditor::Open(std::string_view key, std::filesystem::path const& userDir)
{
	Games::SEditableGame opened{ Games::ReadGameText(userDir, key) };
	Games::EGameSource const source{ Games::FindGameSource(userDir, key) };
	Query::SGameDefinition const* const pGame{ Query::FindGame(key) };

	m_key = key;
	m_name = (pGame != nullptr) ? pGame->name : m_key;
	m_isDownloaded = source == Games::EGameSource::Downloaded || source == Games::EGameSource::Patched;
	m_text = opened.text.empty() ? std::string{ Games::GetNewGameText() } : std::move(opened.text);
	m_openedText = m_text;
	m_note = (m_isDownloaded && !opened.problem.empty()) ? std::format("Your changes cannot be opened, so this is as downloaded: {}", opened.problem) : opened.problem;
	m_isNew = false;
	Opened(userDir);
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::OpenNew(SNewGame game, std::filesystem::path const& userDir)
{
	m_key = std::move(game.key);
	m_name = m_key;
	m_text = std::move(game.text);
	m_openedText = m_text;
	m_note.clear();
	m_isDownloaded = false;
	m_isNew = true;
	Opened(userDir);
}

//////////////////////////////////////////////////////////////////////////
// Escape is taken before the text sees it: there it would undo every change since the text was clicked.
bool CGameEditor::Draw(std::filesystem::path const& userDir, std::string& message)
{
	ImGuiViewport const* const pViewport{ ImGui::GetMainViewport() };
	float const em{ ImGui::GetFontSize() };
	std::array<char, 160> title{};
	bool isSaved{ false };

	if (m_shouldOpen)
	{
		ImGui::OpenPopup(PopupId);
		m_footerHeight = 0.0f;
		m_shouldOpen = false;
	}

	ImGui::SetNextWindowPos(pViewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
	ImGui::SetNextWindowSize(ImVec2{ pViewport->WorkSize.x * SizeFraction, pViewport->WorkSize.y * SizeFraction }, ImGuiCond_Appearing);
	ImGui::SetNextWindowSizeConstraints(ImVec2{ MinWidthEm * em, MinHeightEm * em }, pViewport->WorkSize);

	if (ImGui::BeginPopupModal(m_isNew ? FormatTo(title, "New game: {}{}", m_key, PopupId).data() : FormatTo(title, "{}{}", m_name, PopupId).data()))
	{
		bool const wantsClose{ ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) };
		bool const isEdited{ m_text != m_openedText };
		float const footerEstimate{ ImGui::GetTextLineHeightWithSpacing() * 2.0f + ImGui::GetFrameHeightWithSpacing() };
		float const bodyHeight{ std::max(em, ImGui::GetContentRegionAvail().y - ((m_footerHeight > 0.0f) ? m_footerHeight : footerEstimate)) };
		float const width{ ImGui::GetContentRegionAvail().x };

		if (ImGui::InputTextMultiline("##text", &m_text, ImVec2{ width * TextFraction, bodyHeight }, ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_WordWrap))
		{
			Check();
		}

		ImGui::SameLine();
		ImGui::BeginChild("##reference", ImVec2{ 0.0f, bodyHeight }, ImGuiChildFlags_Borders);
		DrawGameReference();
		ImGui::EndChild();

		float const footerStart{ ImGui::GetCursorPosY() };

		DrawStatus(width);
		ImGui::BeginDisabled(!m_problem.empty() || !(m_isNew || isEdited));
		isSaved = ImGui::Button("Save") && Save(userDir, message);
		ImGui::EndDisabled();
		ImGui::SameLine();

		bool const shouldClose{ isSaved || ImGui::Button("Cancel") || (wantsClose && !isEdited) };

		ImGui::SameLine(0.0f, em);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", m_savedAs.c_str());
		m_footerHeight = ImGui::GetCursorPosY() - footerStart;

		if (shouldClose)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return isSaved;
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::Opened(std::filesystem::path const& userDir)
{
	std::string const file{ (userDir / "games" / m_key / "game.json").string() };

	m_savedAs = m_isDownloaded ? std::format("Saved as your changes to the download, in {}", file) : std::format("Saved as {}", file);
	m_shouldOpen = true;
	Check();
}

//////////////////////////////////////////////////////////////////////////
// A failed save is shown until the next edit, which may well be what fixes it.
void CGameEditor::Check()
{
	std::expected<void, std::string> const checked{ Games::CheckGameText(m_text, Query::GetProtocolCatalog()) };

	m_problem = checked.has_value() ? std::string{} : checked.error();
	m_saveError.clear();
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::DrawStatus(float width) const
{
	SThemeColors const& colors{ GetThemeColors() };

	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);

	if (!m_note.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, colors.amber);
		ImGui::TextUnformatted(m_note.data(), m_note.data() + m_note.size());
		ImGui::PopStyleColor();
	}

	if (!m_saveError.empty() || !m_problem.empty())
	{
		std::string const& problem{ m_saveError.empty() ? m_problem : m_saveError };

		ImGui::PushStyleColor(ImGuiCol_Text, colors.error);
		ImGui::TextUnformatted(problem.data(), problem.data() + problem.size());
		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Text, colors.pingGood);
		ImGui::TextUnformatted(LKT_ICON_CHECK " Lookout can use this description");
		ImGui::PopStyleColor();
	}

	ImGui::PopTextWrapPos();
}

//////////////////////////////////////////////////////////////////////////
bool CGameEditor::Save(std::filesystem::path const& userDir, std::string& message)
{
	std::expected<void, std::string> const saved{ Games::SaveGame(userDir, m_key, m_text) };

	if (saved.has_value())
	{
		message = m_isNew ? std::format("Added the game {}", m_key) : std::format("Saved the description of {}", m_name);
	}
	else
	{
		gLog.Warning("Cannot save the description of {}: {}", m_name, saved.error());
		m_saveError = std::format("Not saved: {}", saved.error());
	}

	return saved.has_value();
}
} // namespace Lkt::Ui
