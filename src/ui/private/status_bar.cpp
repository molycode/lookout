#include "status_bar.hpp"
#include "format_to.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "browser/browser.hpp"
#include "net/master_failed.hpp"
#include "query/game.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/master_endpoint.hpp"
#include <imgui.h>
#include <array>
#include <span>

namespace Lkt::Ui
{
//////////////////////////////////////////////////////////////////////////
void DrawStatusBar(Browser::CBrowser const& browser, std::string_view message)
{
	SThemeColors const& colors{ GetThemeColors() };
	bool const hasGame{ browser.GetSelectedGame() != Query::NoGame };
	std::array<char, 96> buffer{};

	if (hasGame)
	{
		Query::SGameDefinition const& game{ Query::GetGame(browser.GetSelectedGame()) };
		Browser::SGameStatus const& status{ browser.GetStatus(game.game) };
		std::span<Net::SMasterFailed const> const failedMasters{ browser.GetFailedMasters(game.game) };
		std::string_view const summary{ status.hasRefreshed
			? FormatTo(buffer, "{} of {} servers answered · {} {}", status.numAnswered, status.numListed, status.numPlayers,
				(status.numPlayers == 1) ? "player" : "players")
			: std::string_view{ "Not refreshed yet" } };

		ImGui::PushStyleColor(ImGuiCol_Text, colors.textDisabled);
		ImGui::TextUnformatted(summary.data(), summary.data() + summary.size());
		ImGui::PopStyleColor();

		if (!failedMasters.empty())
		{
			std::string_view const failed{ FormatTo(buffer, LKT_ICON_WARNING " {} {} failed", failedMasters.size(),
				(failedMasters.size() == 1) ? "master" : "masters") };

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, colors.amber);
			ImGui::TextUnformatted(failed.data(), failed.data() + failed.size());
			ImGui::PopStyleColor();

			if (ImGui::BeginItemTooltip())
			{
				std::array<char, 256> line{};

				for (Net::SMasterFailed const& failure : failedMasters)
				{
					Query::SMasterEndpoint const& master{ game.masters[failure.index] };
					std::string_view const text{ FormatTo(line, "{}:{} {}", master.host, master.port, failure.reason) };

					ImGui::TextUnformatted(text.data(), text.data() + text.size());
				}

				ImGui::EndTooltip();
			}
		}
	}

	if (!message.empty())
	{
		if (hasGame)
		{
			ImGui::SameLine(0.0f, ImGui::GetFontSize() * 2.0f);
		}

		ImGui::TextUnformatted(message.data(), message.data() + message.size());
	}
}
} // namespace Lkt::Ui
