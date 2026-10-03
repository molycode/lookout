#include "join_with_menu.hpp"
#include "format_to.hpp"
#include "frame_intents.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "browser/browser.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Ui
{
namespace
{
constexpr char const* PopupId{ "##join-with" };

//////////////////////////////////////////////////////////////////////////
void DrawItem(std::string_view id, std::string_view name, std::string_view location, std::optional<Launch::ELaunchError> error, uint64_t key,
	SFrameIntents& intents)
{
	ImGui::PushID(id.data(), id.data() + id.size());
	ImGui::BeginDisabled(error.has_value());

	if (SelectableLauncher(name, location))
	{
		intents.action = SServerAction{ EServerAction::Join, key, std::string{ id } };
	}

	ImGui::EndDisabled();

	if (error.has_value())
	{
		std::string_view const reason{ Launch::ToString(*error) };

		ImGui::SetItemTooltip("%.*s", static_cast<int>(reason.size()), reason.data());
	}

	ImGui::PopID();
}
} // namespace

//////////////////////////////////////////////////////////////////////////
size_t CountUsableLaunchers(Browser::CBrowser const& browser)
{
	Query::EGame const game{ browser.GetSelectedGame() };
	std::span<Browser::SInstallLauncher const> const installs{ browser.GetInstallLaunchers(game) };

	return browser.GetLaunchOptions(game).size()
		+ static_cast<size_t>(std::ranges::count_if(installs, [](Browser::SInstallLauncher const& install) { return install.option.has_value(); }));
}

//////////////////////////////////////////////////////////////////////////
void DrawJoinWithItems(Browser::CBrowser const& browser, uint64_t key, SFrameIntents& intents)
{
	Query::EGame const game{ browser.GetSelectedGame() };
	std::span<Launch::SLaunchOption const> const options{ browser.GetLaunchOptions(game) };
	std::span<Browser::SInstallLauncher const> const installs{ browser.GetInstallLaunchers(game) };

	for (Launch::SLaunchOption const& option : options)
	{
		DrawItem(option.id, option.name, option.location, std::nullopt, key, intents);
	}

	for (Browser::SInstallLauncher const& install : installs)
	{
		std::optional<Launch::ELaunchError> const error{ install.option.has_value() ? std::nullopt : std::optional{ install.option.error() } };

		DrawItem(install.id, install.name, install.location, error, key, intents);
	}

	if (options.empty() && installs.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDisabled);
		ImGui::TextUnformatted("No way to start the game was found");
		ImGui::PopStyleColor();
	}
}

//////////////////////////////////////////////////////////////////////////
void OpenJoinWithPopup()
{
	ImGui::OpenPopup(PopupId);
}

//////////////////////////////////////////////////////////////////////////
void DrawJoinWithPopup(Browser::CBrowser const& browser, uint64_t key, SFrameIntents& intents)
{
	if (ImGui::BeginPopup(PopupId))
	{
		DrawJoinWithItems(browser, key, intents);
		ImGui::EndPopup();
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawJoinTooltip(Browser::CBrowser const& browser)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
	{
		Query::EGame const game{ browser.GetSelectedGame() };
		std::expected<Launch::SLaunchOption, Launch::ELaunchError> const& launcher{ browser.GetJoinLauncher(game) };
		std::array<char, 320> buffer{};
		std::string_view const tooltip{ launcher.has_value()
			? FormatTo(buffer, "Starts {} · {}", launcher->name, launcher->location)
			: FormatTo(buffer, "Cannot start {}: {}. Add an install with " LKT_ICON_GEAR " in the sidebar.", Query::GetGame(game).name, Launch::ToString(launcher.error())) };

		ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}
}
} // namespace Lkt::Ui
