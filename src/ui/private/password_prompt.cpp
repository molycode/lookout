#include "password_prompt.hpp"
#include "join_message.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "browser/browser.hpp"
#include "launch/describe_password_rules.hpp"
#include "launch/launch_error.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <format>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "Server password###password" };
} // namespace

//////////////////////////////////////////////////////////////////////////
void CPasswordPrompt::Open(Query::SServerAddress const& joinAddress, std::string serverName, std::string launcherId)
{
	m_joinAddress = joinAddress;
	m_serverName = std::move(serverName);
	m_launcherId = std::move(launcherId);
	m_password.clear();
	m_error.clear();
	m_shouldOpen = true;
}

//////////////////////////////////////////////////////////////////////////
// Escape is read without key ownership, so one press closes the prompt even while the field is being typed in.
void CPasswordPrompt::Draw(Browser::CBrowser& browser, std::string& message)
{
	float const width{ ImGui::GetFontSize() * 20.0f };

	if (m_shouldOpen)
	{
		ImGui::OpenPopup(PopupId);
		m_shouldOpen = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushTextWrapPos(width);
		ImGui::TextUnformatted(m_serverName.data(), m_serverName.data() + m_serverName.size());

		if (ImGui::IsWindowAppearing() || m_shouldFocus)
		{
			ImGui::SetKeyboardFocusHere();
			m_shouldFocus = false;
		}

		ImGui::SetNextItemWidth(width);

		bool isSubmitted{ ImGui::InputText("##password", &m_password, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) };

		if (!m_error.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().error);
			ImGui::TextUnformatted(m_error.data(), m_error.data() + m_error.size());
			ImGui::PopStyleColor();
		}

		ImGui::PopTextWrapPos();
		ImGui::BeginDisabled(m_password.empty());
		isSubmitted = ImGui::Button("Join") || isSubmitted;
		ImGui::EndDisabled();
		ImGui::SameLine();

		bool shouldClose{ ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) };

		if (isSubmitted && !m_password.empty())
		{
			Query::EGame const game{ browser.GetSelectedGame() };
			std::expected<void, Launch::ELaunchError> const joined{ browser.Join(m_joinAddress, m_password, m_launcherId) };

			if (!joined.has_value() && joined.error() == Launch::ELaunchError::UnsupportedPassword)
			{
				m_error = std::format("{}: it must be {}", Launch::ToString(joined.error()), Launch::DescribePasswordRules(Query::GetGame(game).join.password));
				m_shouldFocus = true;
			}
			else
			{
				message = DescribeJoin(Query::GetGame(game).name, m_joinAddress, browser.ResolveLauncher(game, m_launcherId), joined);
				shouldClose = true;
			}
		}

		if (shouldClose)
		{
			m_password.clear();
			m_error.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}
} // namespace Lkt::Ui
