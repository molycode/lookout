#include "add_server_prompt.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "browser/browser.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "Add server###add-server" };
} // namespace

//////////////////////////////////////////////////////////////////////////
void CAddServerPrompt::Open()
{
	m_text.clear();
	m_error.clear();
	m_shouldOpen = true;
}

//////////////////////////////////////////////////////////////////////////
std::optional<Query::SServerAddress> CAddServerPrompt::Draw(Browser::CBrowser& browser)
{
	std::optional<Query::SServerAddress> added{};
	float const width{ ImGui::GetFontSize() * 18.0f };

	if (m_shouldOpen)
	{
		ImGui::OpenPopup(PopupId);
		m_shouldOpen = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Kept as a favourite of the selected game.");

		if (ImGui::IsWindowAppearing() || m_shouldFocus)
		{
			ImGui::SetKeyboardFocusHere();
			m_shouldFocus = false;
		}

		ImGui::SetNextItemWidth(width);

		bool isSubmitted{ ImGui::InputTextWithHint("##address", "a.b.c.d:port", &m_text, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) };

		if (!m_error.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().error);
			ImGui::TextUnformatted(m_error.data(), m_error.data() + m_error.size());
			ImGui::PopStyleColor();
		}

		ImGui::BeginDisabled(m_text.empty());
		isSubmitted = ImGui::Button("Add") || isSubmitted;
		ImGui::EndDisabled();
		ImGui::SameLine();

		bool shouldClose{ ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) };

		if (isSubmitted && !m_text.empty())
		{
			std::expected<Query::SServerAddress, Query::EParseError> const address{ browser.AddServer(m_text) };

			if (address.has_value())
			{
				added = *address;
				shouldClose = true;
			}
			else
			{
				m_error = "Not an address: use a.b.c.d:port";
				m_shouldFocus = true;
			}
		}

		if (shouldClose)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return added;
}
} // namespace Lkt::Ui
