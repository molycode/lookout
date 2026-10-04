#include "game_sidebar.hpp"
#include "format_to.hpp"
#include "frame_intents.hpp"
#include "game_icons.hpp"
#include "game_move.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "browser/browser.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Ui
{
namespace
{
constexpr float CardRoundingEm{ 0.5f };
constexpr float SelectedFillAlpha{ 0.16f };
constexpr float HoveredSelectedFillAlpha{ 0.22f };
constexpr float ActiveSelectedFillAlpha{ 0.30f };
constexpr char const* GamePayload{ "game" };

//////////////////////////////////////////////////////////////////////////
void DrawGearButton(Query::SGameDefinition const& game, SFrameIntents& intents)
{
	std::array<char, 96> buffer{};

	if (IconButton("##settings", LKT_ICON_GEAR))
	{
		intents.openGameSettings = game.game;
	}

	std::string_view const tooltip{ FormatTo(buffer, "How Lookout starts {}", game.name) };

	ImGui::SetItemTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
}

//////////////////////////////////////////////////////////////////////////
void DragToReorder(Query::EGame game, SFrameIntents& intents)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip))
	{
		ImGui::SetDragDropPayload(GamePayload, &game, sizeof(game));
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		ImGuiPayload const* const pPayload{ ImGui::AcceptDragDropPayload(GamePayload,
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect) };

		if (pPayload != nullptr)
		{
			Query::EGame dragged{ Query::NoGame };

			std::memcpy(&dragged, pPayload->Data, sizeof(dragged));

			if (dragged != game)
			{
				intents.moveGame = SGameMove{ dragged, game };
			}
		}

		ImGui::EndDragDropTarget();
	}
}

//////////////////////////////////////////////////////////////////////////
// The selected card is a muted amber tint, so its amber name stands out on it.
ImU32 GetCardFill(bool isSelected, bool isHovered, bool isActive)
{
	ImU32 fill{ ImGui::GetColorU32(ImGuiCol_FrameBg) };

	if (isSelected)
	{
		ImVec4 const& amber{ GetThemeColors().amber };
		float alpha{ SelectedFillAlpha };

		if (isActive)
		{
			alpha = ActiveSelectedFillAlpha;
		}
		else if (isHovered)
		{
			alpha = HoveredSelectedFillAlpha;
		}

		fill = ImGui::GetColorU32(ImVec4{ amber.x, amber.y, amber.z, alpha });
	}
	else if (isActive)
	{
		fill = ImGui::GetColorU32(ImGuiCol_FrameBgActive);
	}
	else if (isHovered)
	{
		fill = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
	}

	return fill;
}

//////////////////////////////////////////////////////////////////////////
// A group, so the layout carries on below the card.
void DrawListedGame(Query::SGameDefinition const& game, Browser::CBrowser const& browser, bool isLastListed, SFrameIntents& intents)
{
	SThemeColors const& colors{ GetThemeColors() };
	ImGuiStyle const& style{ ImGui::GetStyle() };
	Browser::SGameStatus const& status{ browser.GetStatus(game.game) };
	bool const isSelected{ game.game == browser.GetSelectedGame() };
	float const lineHeight{ ImGui::GetTextLineHeight() };
	ImVec2 const padding{ style.FramePadding };
	ImVec2 const start{ ImGui::GetCursorScreenPos() };
	float const iconSize{ ImGui::GetTextLineHeightWithSpacing() + lineHeight };
	ImVec2 const size{ ImGui::GetContentRegionAvail().x, padding.y * 2.0f + iconSize };
	ImVec2 const icon{ start.x + padding.x, start.y + padding.y };
	ImVec2 const text{ icon.x + iconSize + style.ItemInnerSpacing.x, icon.y };
	float const gearX{ start.x + size.x - padding.x - lineHeight };
	float const hideX{ gearX - style.ItemInnerSpacing.x - lineHeight };
	float const spinnerWidth{ status.isRefreshing ? ImGui::CalcTextSize(LKT_ICON_ROTATE).x + style.ItemInnerSpacing.x : 0.0f };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };
	std::array<char, 96> buffer{};

	ImGui::BeginGroup();
	ImGui::SetNextItemAllowOverlap();

	if (ImGui::InvisibleButton("##game", size, ImGuiButtonFlags_EnableNav) && !isSelected)
	{
		intents.selectGame = game.game;
	}

	DragToReorder(game.game, intents);

	bool const isHovered{ ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem) };

	pDrawList->AddRectFilled(start, ImVec2{ start.x + size.x, start.y + size.y }, GetCardFill(isSelected, isHovered, ImGui::IsItemActive()),
		ImGui::GetFontSize() * CardRoundingEm);
	gGameIcons.Draw(pDrawList, game.game, icon, iconSize);

	float const nameEnd{ DrawEllipsised(game.name, text, hideX - style.ItemInnerSpacing.x - spinnerWidth, isSelected ? colors.amber : colors.text) };

	if (status.isRefreshing)
	{
		pDrawList->AddText(ImVec2{ nameEnd + style.ItemInnerSpacing.x, text.y }, ImGui::GetColorU32(colors.amber), LKT_ICON_ROTATE);
	}

	std::string_view const counts{ status.hasRefreshed
		? FormatTo(buffer, "{} {} · {} {}", status.numAnswered, (status.numAnswered == 1) ? "server" : "servers",
			status.numPlayers, (status.numPlayers == 1) ? "player" : "players")
		: std::string_view{ "—" } };

	DrawEllipsised(counts, ImVec2{ text.x, text.y + ImGui::GetTextLineHeightWithSpacing() }, start.x + size.x - padding.x, colors.textDisabled);
	ImGui::SetCursorScreenPos(ImVec2{ hideX, text.y });
	ImGui::BeginDisabled(isLastListed);

	if (IconButton("##hide", LKT_ICON_EYE_SLASH))
	{
		intents.hideGame = game.game;
	}

	ImGui::EndDisabled();

	std::string_view const hideTooltip{ isLastListed ? std::string_view{ "One game always stays in the list" } : FormatTo(buffer, "Hide {}", game.name) };

	ImGui::SetItemTooltip("%.*s", static_cast<int>(hideTooltip.size()), hideTooltip.data());
	ImGui::SetCursorScreenPos(ImVec2{ gearX, text.y });
	DrawGearButton(game, intents);
	ImGui::EndGroup();
}

//////////////////////////////////////////////////////////////////////////
void DrawProblems(std::span<std::string const> problems)
{
	if (!problems.empty())
	{
		std::array<char, 48> buffer{};
		std::string_view const label{ (problems.size() == 1) ? FormatTo(buffer, LKT_ICON_WARNING " 1 problem###problems")
			: FormatTo(buffer, LKT_ICON_WARNING " {} problems###problems", problems.size()) };

		ImGui::Spacing();

		if (ImGui::TreeNodeEx(label.data(), ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().amber);
			ImGui::PushTextWrapPos(0.0f);

			for (std::string const& problem : problems)
			{
				ImGui::TextUnformatted(problem.data(), problem.data() + problem.size());
			}

			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Flat and not selectable, since the selected game is always a listed one; inset to line up with the cards' text.
void DrawHiddenGame(Query::SGameDefinition const& game, SFrameIntents& intents)
{
	ImGuiStyle const& style{ ImGui::GetStyle() };
	float const lineHeight{ ImGui::GetTextLineHeight() };
	ImVec2 const start{ ImGui::GetCursorScreenPos() };
	float const gearX{ start.x + ImGui::GetContentRegionAvail().x - style.FramePadding.x - lineHeight };
	float const showX{ gearX - style.ItemInnerSpacing.x - lineHeight };
	ImVec2 const icon{ start.x + style.FramePadding.x, start.y };
	std::array<char, 96> buffer{};

	gGameIcons.Draw(ImGui::GetWindowDrawList(), game.game, icon, lineHeight);
	DrawEllipsised(game.name, ImVec2{ icon.x + lineHeight + style.ItemInnerSpacing.x, start.y }, showX - style.ItemInnerSpacing.x, GetThemeColors().textDisabled);
	ImGui::SetCursorScreenPos(ImVec2{ showX, start.y });

	if (IconButton("##show", LKT_ICON_EYE))
	{
		intents.showGame = game.game;
	}

	std::string_view const tooltip{ FormatTo(buffer, "Show {}", game.name) };

	ImGui::SetItemTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
	DrawGearButton(game, intents);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void DrawGameSidebar(Browser::CBrowser const& browser, SFrameIntents& intents)
{
	std::span<Config::SGameSettings const> const games{ browser.GetSettings().games };
	std::span<Query::EGame const> const order{ browser.GetSettings().gameOrder };
	size_t const numListed{ static_cast<size_t>(std::ranges::count(games, true, &Config::SGameSettings::isListed)) };
	size_t const numHidden{ games.size() - numListed };

	for (Query::EGame const game : order)
	{
		if (games[static_cast<size_t>(game)].isListed)
		{
			ImGui::PushID(static_cast<int>(game));
			DrawListedGame(Query::GetGame(game), browser, numListed == 1, intents);
			ImGui::PopID();
		}
	}

	if (numHidden > 0)
	{
		std::array<char, 48> buffer{};
		std::string_view const label{ (numHidden == 1) ? FormatTo(buffer, "1 hidden game###hidden") : FormatTo(buffer, "{} hidden games###hidden", numHidden) };

		ImGui::Spacing();

		if (ImGui::TreeNodeEx(label.data(), ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			for (Query::EGame const game : order)
			{
				if (!games[static_cast<size_t>(game)].isListed)
				{
					ImGui::PushID(static_cast<int>(game));
					DrawHiddenGame(Query::GetGame(game), intents);
					ImGui::PopID();
				}
			}
		}
	}

	DrawProblems(browser.GetGameProblems());
}
} // namespace Lkt::Ui
