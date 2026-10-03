#include "status_bar.hpp"
#include "format_to.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "browser/browser.hpp"
#include <imgui.h>
#include <array>

namespace Lkt::Ui
{
//////////////////////////////////////////////////////////////////////////
void DrawStatusBar(Browser::CBrowser const& browser, std::string_view message)
{
	Browser::SGameStatus const& status{ browser.GetStatus(browser.GetSelectedGame()) };
	SThemeColors const& colors{ GetThemeColors() };
	std::array<char, 96> buffer{};

	std::string_view const summary{ status.hasRefreshed
		? FormatTo(buffer, "{} of {} servers answered · {} players", status.numAnswered, status.numListed, status.numPlayers)
		: std::string_view{ "Not refreshed yet" } };

	ImGui::PushStyleColor(ImGuiCol_Text, colors.textDisabled);
	ImGui::TextUnformatted(summary.data(), summary.data() + summary.size());
	ImGui::PopStyleColor();

	if (status.numMastersFailed > 0)
	{
		std::string_view const failed{ FormatTo(buffer, LKT_ICON_WARNING " {} {} not answer", status.numMastersFailed,
			(status.numMastersFailed == 1) ? "master did" : "masters did") };

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, colors.amber);
		ImGui::TextUnformatted(failed.data(), failed.data() + failed.size());
		ImGui::PopStyleColor();
	}

	if (!message.empty())
	{
		ImGui::SameLine(0.0f, ImGui::GetFontSize() * 2.0f);
		ImGui::TextUnformatted(message.data(), message.data() + message.size());
	}
}
} // namespace Lkt::Ui
