#include "about_dialog.hpp"
#include "format_to.hpp"
#include "licensed_components.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "launch/file_url.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <string_view>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "About Lookout###about" };
constexpr float PageEm{ 35.0f };
constexpr float PageLines{ 11.0f };
constexpr float ListEm{ 11.0f };
constexpr float TitleScale{ 1.6f };

//////////////////////////////////////////////////////////////////////////
std::string_view OrNone(char const* pText)
{
	return (pText != nullptr) ? std::string_view{ pText } : std::string_view{ "none" };
}

//////////////////////////////////////////////////////////////////////////
std::string_view OrNone(std::string_view text)
{
	return text.empty() ? std::string_view{ "none" } : text;
}

//////////////////////////////////////////////////////////////////////////
void DrawDisabledText(std::string_view text)
{
	ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDisabled);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopStyleColor();
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CAboutDialog::Initialize(SDL_Window* pWindow, SAboutInfo const& about)
{
	m_pWindow = pWindow;
	m_about = about;
}

//////////////////////////////////////////////////////////////////////////
void CAboutDialog::Open()
{
	int const sdlVersion{ SDL_GetVersion() };

	m_systemInfo = std::format("Lookout {}\nSDL {}.{}.{} · video {} · renderer {} · UI scale {:.2f}\nDear ImGui {}\nConfig {}\nLogs {}",
		m_about.version, SDL_VERSIONNUM_MAJOR(sdlVersion), SDL_VERSIONNUM_MINOR(sdlVersion), SDL_VERSIONNUM_MICRO(sdlVersion),
		OrNone(SDL_GetCurrentVideoDriver()), OrNone(SDL_GetRendererName(SDL_GetRenderer(m_pWindow))), ImGui::GetStyle().FontScaleDpi,
		IMGUI_VERSION, OrNone(m_about.configDir), OrNone(m_about.logsDir));
	m_result.clear();
	m_shouldOpen = true;
}

//////////////////////////////////////////////////////////////////////////
void CAboutDialog::Draw()
{
	ImGuiViewport const* const pViewport{ ImGui::GetMainViewport() };
	ImVec2 const pageSize{ PageEm * ImGui::GetFontSize(), ImGui::GetTextLineHeightWithSpacing() * PageLines };

	if (m_shouldOpen)
	{
		ImGui::OpenPopup(PopupId);
		m_shouldOpen = false;
	}

	ImGui::SetNextWindowPos(pViewport->GetWorkCenter(), ImGuiCond_Always, ImVec2{ 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

		if (ImGui::BeginTabBar("##tabs"))
		{
			if (ImGui::BeginTabItem("About"))
			{
				ImGui::BeginChild("##page", pageSize, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
				DrawAbout();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("System"))
			{
				ImGui::BeginChild("##page", pageSize);
				DrawSystem();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Licences"))
			{
				DrawLicences(pageSize);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::PopStyleColor();

		if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAboutDialog::DrawAbout()
{
	std::array<char, 64> buffer{};

	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * TitleScale);
	ImGui::TextUnformatted("Lookout");
	ImGui::PopFont();
	DrawDisabledText(FormatTo(buffer, "Version {}", m_about.version));
	ImGui::Spacing();
	ImGui::TextWrapped("Finds game servers, shows who is playing, and joins through the game's own launcher.");
	ImGui::Spacing();
	DrawDisabledText("© 2026 moly · MIT License");
}

//////////////////////////////////////////////////////////////////////////
void CAboutDialog::DrawSystem()
{
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(m_systemInfo.data(), m_systemInfo.data() + m_systemInfo.size());
	ImGui::PopTextWrapPos();
	ImGui::Spacing();

	if (ImGui::Button("Copy"))
	{
		m_isResultError = !SDL_SetClipboardText(m_systemInfo.c_str());
		m_result = m_isResultError ? "Cannot copy to the clipboard" : "Copied";

		if (m_isResultError)
		{
			gLog.Warning("Cannot copy the system info to the clipboard: {}", SDL_GetError());
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(m_about.logsDir.empty());

	if (ImGui::Button("Open logs folder"))
	{
		std::string const url{ Launch::ToFileUrl(m_about.logsDir) };

		m_isResultError = !SDL_OpenURL(url.c_str());
		m_result = m_isResultError ? "Cannot open the logs folder" : std::string{};

		if (m_isResultError)
		{
			gLog.Warning("Cannot open the logs folder '{}': {}", m_about.logsDir, SDL_GetError());
		}
	}

	ImGui::EndDisabled();

	if (!m_result.empty())
	{
		SThemeColors const& colors{ GetThemeColors() };

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, m_isResultError ? colors.error : colors.textDisabled);
		ImGui::TextUnformatted(m_result.data(), m_result.data() + m_result.size());
		ImGui::PopStyleColor();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAboutDialog::DrawLicences(ImVec2 const& pageSize)
{
	std::span<SLicensedComponent const> const components{ GetLicensedComponents() };
	float const listWidth{ ListEm * ImGui::GetFontSize() };

	ImGui::BeginChild("##components", ImVec2{ listWidth, pageSize.y });

	for (size_t index{ 0 }; index < components.size(); ++index)
	{
		ImGui::PushID(static_cast<int>(index));

		if (SelectableText(components[index].name, index == m_licenceIndex))
		{
			m_licenceIndex = index;
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
	ImGui::SameLine();

	SLicensedComponent const& component{ components[m_licenceIndex] };
	char const* const pText{ reinterpret_cast<char const*>(component.text.data()) };

	ImGui::BeginChild("##licence", ImVec2{ pageSize.x - listWidth - ImGui::GetStyle().ItemSpacing.x, pageSize.y }, ImGuiChildFlags_Borders);
	DrawDisabledText(component.licence);
	ImGui::Spacing();
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(pText, pText + component.text.size());
	ImGui::PopTextWrapPos();
	ImGui::EndChild();
}
} // namespace Lkt::Ui
