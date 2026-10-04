#include "discard_prompt.hpp"
#include "loggers.hpp"
#include "games/game_files.hpp"
#include <imgui.h>
#include <expected>
#include <format>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "###discard" };
} // namespace

//////////////////////////////////////////////////////////////////////////
void CDiscardPrompt::Open(EDiscardKind kind, std::string key, std::string name, std::filesystem::path const& userDir)
{
	m_kind = kind;
	m_key = std::move(key);
	m_name = std::move(name);
	m_question = (kind == EDiscardKind::Changes) ? std::format("Discard your changes to {}?", m_name)
		: std::format("Remove {}? This deletes its folder and everything in it:\n{}", m_name, (userDir / "games" / m_key).string());
	m_shouldOpen = true;
}

//////////////////////////////////////////////////////////////////////////
bool CDiscardPrompt::Draw(std::filesystem::path const& userDir, std::string& message)
{
	bool const isChanges{ m_kind == EDiscardKind::Changes };
	float const width{ ImGui::GetFontSize() * 22.0f };
	bool isConfirmed{ false };

	if (m_shouldOpen)
	{
		ImGui::OpenPopup(PopupId);
		m_shouldOpen = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(isChanges ? "Revert to built-in###discard" : "Remove game###discard", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushTextWrapPos(width);
		ImGui::TextUnformatted(m_question.data(), m_question.data() + m_question.size());
		ImGui::PopTextWrapPos();
		ImGui::Spacing();

		isConfirmed = ImGui::Button(isChanges ? "Discard" : "Remove");
		ImGui::SameLine();

		bool const shouldClose{ isConfirmed || ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) };

		if (isConfirmed)
		{
			Discard(userDir, message);
		}

		if (shouldClose)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return isConfirmed;
}

//////////////////////////////////////////////////////////////////////////
void CDiscardPrompt::Discard(std::filesystem::path const& userDir, std::string& message) const
{
	bool const isChanges{ m_kind == EDiscardKind::Changes };
	std::expected<void, std::string> const discarded{ isChanges ? Games::RevertGame(userDir, m_key) : Games::RemoveGame(userDir, m_key) };

	if (discarded.has_value())
	{
		message = isChanges ? std::format("{} is the built-in again", m_name) : std::format("Removed {}", m_name);
	}
	else
	{
		gLog.Warning("Cannot {} {}: {}", isChanges ? "revert" : "remove", m_name, discarded.error());
		message = std::format("Cannot {} {}: {}", isChanges ? "revert" : "remove", m_name, discarded.error());
	}
}
} // namespace Lkt::Ui
