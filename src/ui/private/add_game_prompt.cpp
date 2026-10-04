#include "add_game_prompt.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "games/game_files.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <format>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "Add game###add-game" };
constexpr char const* BlankLabel{ "A blank description" };

//////////////////////////////////////////////////////////////////////////
// Typed capitals become small letters and anything a key cannot hold is dropped, so most keys are valid as typed.
int FilterKeyCharacter(ImGuiInputTextCallbackData* pData)
{
	ImWchar const c{ pData->EventChar };
	bool const isUpper{ c >= 'A' && c <= 'Z' };

	if (isUpper)
	{
		pData->EventChar = static_cast<ImWchar>(c - 'A' + 'a');
	}

	bool const isKept{ isUpper || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' };

	return isKept ? 0 : 1;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CAddGamePrompt::Open()
{
	m_key.clear();
	m_startFrom.clear();
	m_error.clear();
	m_shouldOpen = true;
}

//////////////////////////////////////////////////////////////////////////
std::optional<SNewGame> CAddGamePrompt::Draw(std::filesystem::path const& userDir)
{
	std::optional<SNewGame> added{};
	float const width{ ImGui::GetFontSize() * 18.0f };

	if (m_shouldOpen)
	{
		ImGui::OpenPopup(PopupId);
		m_shouldOpen = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Its name for folders and settings:");

		if (ImGui::IsWindowAppearing() || m_shouldFocus)
		{
			ImGui::SetKeyboardFocusHere();
			m_shouldFocus = false;
		}

		ImGui::SetNextItemWidth(width);

		bool isSubmitted{ ImGui::InputTextWithHint("##key", "my-game", &m_key, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter,
			FilterKeyCharacter) };

		ImGui::TextDisabled("Small letters, digits, '-' and '_'");
		ImGui::Spacing();
		ImGui::TextUnformatted("Start from:");
		ImGui::SetNextItemWidth(width);
		DrawStartFrom();

		if (!m_error.empty())
		{
			ImGui::PushTextWrapPos(width);
			ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().error);
			ImGui::TextUnformatted(m_error.data(), m_error.data() + m_error.size());
			ImGui::PopStyleColor();
			ImGui::PopTextWrapPos();
		}

		ImGui::Spacing();
		ImGui::BeginDisabled(m_key.empty());
		isSubmitted = ImGui::Button("Continue") || isSubmitted;
		ImGui::EndDisabled();
		ImGui::SameLine();

		bool shouldClose{ ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) };

		if (isSubmitted && !m_key.empty())
		{
			added = Submit(userDir);
			shouldClose = added.has_value() || shouldClose;
		}

		if (shouldClose)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return added;
}

//////////////////////////////////////////////////////////////////////////
void CAddGamePrompt::DrawStartFrom()
{
	Query::SGameDefinition const* const pFrom{ Query::FindGame(m_startFrom) };

	if (ImGui::BeginCombo("##start-from", (pFrom != nullptr) ? pFrom->name.c_str() : BlankLabel))
	{
		if (ImGui::Selectable(BlankLabel, pFrom == nullptr))
		{
			m_startFrom.clear();
		}

		for (Query::SGameDefinition const& game : Query::GetGameCatalog())
		{
			ImGui::PushID(game.key.c_str());

			if (ImGui::Selectable(game.name.c_str(), &game == pFrom))
			{
				m_startFrom = game.key;
			}

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}
}

//////////////////////////////////////////////////////////////////////////
std::optional<SNewGame> CAddGamePrompt::Submit(std::filesystem::path const& userDir)
{
	Query::SGameDefinition const* const pSameKey{ Query::FindGame(m_key) };
	Games::EGameSource const source{ Games::IsValidKey(m_key) ? Games::FindGameSource(userDir, m_key) : Games::EGameSource::None };
	std::optional<SNewGame> added{};

	if (!Games::IsValidKey(m_key))
	{
		m_error = "It must start with a small letter or a digit.";
	}
	else if (pSameKey != nullptr)
	{
		m_error = std::format("{} already has this name.", pSameKey->name);
	}
	else if (source == Games::EGameSource::Downloaded || source == Games::EGameSource::Patched)
	{
		m_error = "A downloaded game has this name.";
	}
	else if (source == Games::EGameSource::User)
	{
		m_error = std::format("{} already exists.", (userDir / "games" / m_key).string());
	}
	else
	{
		added = SNewGame{ m_key, m_startFrom.empty() ? std::string{ Games::GetNewGameText() } : Games::ReadGameText(userDir, m_startFrom).text };
	}

	m_shouldFocus = !added.has_value();

	return added;
}
} // namespace Lkt::Ui
