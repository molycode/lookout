#include "server_table.hpp"
#include "flag_atlas.hpp"
#include "format_to.hpp"
#include "frame_intents.hpp"
#include "icons.hpp"
#include "join_with_menu.hpp"
#include "selection.hpp"
#include "styled_text_view.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "browser/browser.hpp"
#include "geo/countries.hpp"
#include <tge/assert.hpp>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdint>

namespace Lkt::Ui
{
namespace
{
constexpr int NumColumns{ 9 };
constexpr int NameColumn{ 2 };
constexpr int CountryColumn{ 8 };
constexpr uint32_t GoodPingMs{ 80 };
constexpr uint32_t FairPingMs{ 150 };

//////////////////////////////////////////////////////////////////////////
void DrawText(std::string_view text, ImVec4 const& color)
{
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopStyleColor();
}

//////////////////////////////////////////////////////////////////////////
std::string_view DescribeState(Browser::EServerState state)
{
	std::string_view text{ "waiting" };

	switch (state)
	{
		case Browser::EServerState::Pending:
			text = "waiting";
			break;
		case Browser::EServerState::Online:
			text = "online";
			break;
		case Browser::EServerState::NoAnswer:
			text = "no answer";
			break;
		case Browser::EServerState::BadReply:
			text = "bad reply";
			break;
	}

	return text;
}

//////////////////////////////////////////////////////////////////////////
ImVec4 const& GetPingColor(uint32_t pingMs)
{
	SThemeColors const& colors{ GetThemeColors() };

	return (pingMs < GoodPingMs) ? colors.pingGood : ((pingMs < FairPingMs) ? colors.pingFair : colors.pingPoor);
}

//////////////////////////////////////////////////////////////////////////
// The saved column opens sorted its saved way; the others keep the direction a first click on them picks.
ImGuiTableColumnFlags GetSortFlags(Config::SSortOrder const& saved, Config::ESortColumn column, bool prefersAscending)
{
	bool const isSaved{ saved.column == column };
	bool const isAscending{ isSaved ? saved.isAscending : prefersAscending };

	return (isSaved ? ImGuiTableColumnFlags_DefaultSort : ImGuiTableColumnFlags_None)
		| (isAscending ? ImGuiTableColumnFlags_PreferSortAscending : ImGuiTableColumnFlags_PreferSortDescending);
}

//////////////////////////////////////////////////////////////////////////
void SetupColumns(Config::SSortOrder const& saved)
{
	float const em{ ImGui::GetFontSize() };
	ImGuiTableColumnFlags const iconColumn{ ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize };
	// Room for the icon and the sort arrow beside it.
	float const iconColumnWidth{ em * 2.2f };

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn(LKT_ICON_STAR "##favourite", iconColumn | ImGuiTableColumnFlags_NoHide | GetSortFlags(saved, Config::ESortColumn::Favourite, false), iconColumnWidth,
		static_cast<ImGuiID>(Config::ESortColumn::Favourite));
	ImGui::TableSetupColumn(LKT_ICON_LOCK "##password", iconColumn | GetSortFlags(saved, Config::ESortColumn::Password, false), iconColumnWidth,
		static_cast<ImGuiID>(Config::ESortColumn::Password));
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide | GetSortFlags(saved, Config::ESortColumn::Name, true), 3.0f,
		static_cast<ImGuiID>(Config::ESortColumn::Name));
	ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch | GetSortFlags(saved, Config::ESortColumn::Map, true), 1.2f,
		static_cast<ImGuiID>(Config::ESortColumn::Map));
	ImGui::TableSetupColumn("Mod", ImGuiTableColumnFlags_WidthStretch | GetSortFlags(saved, Config::ESortColumn::Mod, true), 1.2f,
		static_cast<ImGuiID>(Config::ESortColumn::Mod));
	ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch | GetSortFlags(saved, Config::ESortColumn::Mode, true), 1.0f,
		static_cast<ImGuiID>(Config::ESortColumn::Mode));
	ImGui::TableSetupColumn("Players", ImGuiTableColumnFlags_WidthFixed | GetSortFlags(saved, Config::ESortColumn::Players, false), em * 4.5f,
		static_cast<ImGuiID>(Config::ESortColumn::Players));
	// Fitted rather than given a width, so ImGui always saves every column: 1.92.9b misorders a table saved with its sort alone.
	ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed | GetSortFlags(saved, Config::ESortColumn::Ping, true), 0.0f,
		static_cast<ImGuiID>(Config::ESortColumn::Ping));
	ImGui::TableSetupColumn(LKT_ICON_GLOBE "##country", iconColumn | GetSortFlags(saved, Config::ESortColumn::Country, true), iconColumnWidth,
		static_cast<ImGuiID>(Config::ESortColumn::Country));
	ImGui::TableHeadersRow();
}

//////////////////////////////////////////////////////////////////////////
// ImGui reports the specs dirty on a table's first frame and after it was idle for a while, not only on a click.
void ReadSortSpecs(Config::SSortOrder const& saved, SFrameIntents& intents)
{
	ImGuiTableSortSpecs* const pSpecs{ ImGui::TableGetSortSpecs() };

	if (pSpecs != nullptr && pSpecs->SpecsDirty)
	{
		if (pSpecs->SpecsCount > 0)
		{
			TGE_ASSERT(pSpecs->Specs[0].ColumnUserID < static_cast<ImGuiID>(Config::NumSortColumns), "A column's sort id is outside ESortColumn");

			Config::SSortOrder const order{ static_cast<Config::ESortColumn>(pSpecs->Specs[0].ColumnUserID),
				pSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending };

			if (order != saved)
			{
				intents.sort = order;
			}
		}

		pSpecs->SpecsDirty = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawNameCell(Browser::SServerEntry const& entry)
{
	std::array<char, Query::MaxFormattedAddress> address{};
	std::array<char, 64> buffer{};

	if (entry.state != Browser::EServerState::Online)
	{
		std::string_view const text{ FormatTo(buffer, "{}  ({})", Query::FormatAddressTo(entry.joinAddress, address), DescribeState(entry.state)) };

		DrawText(text, GetThemeColors().textDisabled);
	}
	else if (entry.summary.name.plain.empty())
	{
		std::string_view const text{ Query::FormatAddressTo(entry.joinAddress, address) };

		ImGui::TextUnformatted(text.data(), text.data() + text.size());
	}
	else
	{
		DrawClippedStyledText(entry.summary.name);
	}
}

//////////////////////////////////////////////////////////////////////////
// Drawn rather than an ImGui::ProgressBar: a framed widget lowers the text baseline of the cells after it in the row.
void DrawPlayersBar(float fraction, std::string_view text)
{
	ImVec2 const position{ ImGui::GetCursorScreenPos() };
	ImVec2 const size{ ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() };
	ImVec2 const textSize{ ImGui::CalcTextSize(text.data(), text.data() + text.size()) };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };
	float const rounding{ ImGui::GetStyle().FrameRounding };

	pDrawList->AddRectFilled(position, ImVec2{ position.x + size.x, position.y + size.y }, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding);

	if (fraction > 0.0f)
	{
		pDrawList->AddRectFilled(position, ImVec2{ position.x + size.x * fraction, position.y + size.y }, ImGui::GetColorU32(ImGuiCol_PlotHistogram, 0.55f), rounding);
	}

	pDrawList->AddText(ImVec2{ position.x + (size.x - textSize.x) * 0.5f, position.y }, ImGui::GetColorU32(ImGuiCol_Text), text.data(), text.data() + text.size());
	ImGui::Dummy(size);
}

//////////////////////////////////////////////////////////////////////////
void DrawDetailCells(Browser::SServerEntry const& entry)
{
	std::array<char, 32> buffer{};
	Query::SServerSummary const& summary{ entry.summary };

	if (entry.state == Browser::EServerState::Online)
	{
		float const fraction{ (summary.maxPlayers > 0) ? std::min(1.0f, static_cast<float>(summary.numPlayers) / static_cast<float>(summary.maxPlayers)) : 0.0f };
		std::string_view const players{ (summary.maxPlayers > 0)
			? FormatTo(buffer, "{}/{}", summary.numPlayers, summary.maxPlayers)
			: FormatTo(buffer, "{}/?", summary.numPlayers) };

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(summary.map.data(), summary.map.data() + summary.map.size());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(summary.mod.data(), summary.mod.data() + summary.mod.size());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(summary.mode.data(), summary.mode.data() + summary.mode.size());
		ImGui::TableNextColumn();
		DrawPlayersBar(fraction, players);
		ImGui::TableNextColumn();
		DrawText(FormatTo(buffer, "{}", entry.pingMs), GetPingColor(entry.pingMs));
	}
	else
	{
		for (int column{ NameColumn + 1 }; column < CountryColumn; ++column)
		{
			ImGui::TableNextColumn();
			DrawText("—", GetThemeColors().textDisabled);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawCountryCell(Browser::SServerEntry const& entry)
{
	if (entry.country != Geo::NoCountry)
	{
		std::string_view const name{ Geo::GetCountry(entry.country).name };

		gFlagAtlas.DrawItem(entry.country);
		ImGui::SetItemTooltip("%.*s", static_cast<int>(name.size()), name.data());
	}
}

//////////////////////////////////////////////////////////////////////////
// The star is its own item over the row's selectable, so a click on it toggles without selecting or joining.
void DrawFavouriteToggle(Browser::SServerEntry const& entry, ImVec2 const& position, uint64_t key, SFrameIntents& intents)
{
	SThemeColors const& colors{ GetThemeColors() };

	ImGui::SetCursorScreenPos(position);
	ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);

	if (ImGui::InvisibleButton("##star", ImVec2{ ImGui::GetFontSize(), ImGui::GetTextLineHeight() }))
	{
		intents.action = SServerAction{ EServerAction::ToggleFavourite, key };
	}

	ImGui::PopItemFlag();

	ImVec4 const& color{ entry.isFavourite ? colors.amber : (ImGui::IsItemHovered() ? colors.text : colors.textDisabled) };

	ImGui::GetWindowDrawList()->AddText(ImGui::GetItemRectMin(), ImGui::GetColorU32(color), LKT_ICON_STAR);
}

//////////////////////////////////////////////////////////////////////////
void DrawContextMenu(Browser::CBrowser const& browser, Browser::SServerEntry const& entry, uint64_t key, uint64_t& selectedKey, SFrameIntents& intents)
{
	if (ImGui::BeginPopupContextItem("##actions"))
	{
		selectedKey = key;

		if (CountUsableLaunchers(browser) > 1)
		{
			if (ImGui::BeginMenu(LKT_ICON_PLAY " Join with"))
			{
				DrawJoinWithItems(browser, key, intents);
				ImGui::EndMenu();
			}
		}
		else
		{
			if (ImGui::MenuItem(LKT_ICON_PLAY " Join", nullptr, false, browser.GetJoinLauncher(browser.GetSelectedGame()).has_value()))
			{
				intents.action = SServerAction{ EServerAction::Join, key };
			}

			DrawJoinTooltip(browser);
		}

		if (ImGui::MenuItem(LKT_ICON_ROTATE " Refresh server"))
		{
			intents.action = SServerAction{ EServerAction::Refresh, key };
		}

		if (ImGui::MenuItem(entry.isFavourite ? LKT_ICON_STAR " Remove from favourites" : LKT_ICON_STAR " Add to favourites"))
		{
			intents.action = SServerAction{ EServerAction::ToggleFavourite, key };
		}

		if (ImGui::MenuItem(LKT_ICON_COPY " Copy address"))
		{
			intents.action = SServerAction{ EServerAction::CopyAddress, key };
		}

		ImGui::EndPopup();
	}
}

//////////////////////////////////////////////////////////////////////////
// A double-click joins only the row that was already selected, so a re-sort between the two clicks never joins another.
bool DrawRow(Browser::CBrowser const& browser, Browser::SServerEntry const& entry, uint64_t& selectedKey, bool& shouldScrollToSelection, SFrameIntents& intents)
{
	uint64_t const key{ Query::ToKey(entry.address) };
	bool const isSelected{ key == selectedKey };
	ImGuiSelectableFlags const flags{ ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap
		| ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SelectOnNav };
	bool isDoubleClicked{ false };

	ImGui::PushID(reinterpret_cast<void const*>(static_cast<uintptr_t>(key)));
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImVec2 const start{ ImGui::GetCursorScreenPos() };

	if (ImGui::Selectable("##row", isSelected, flags))
	{
		isDoubleClicked = isSelected && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		selectedKey = key;
	}

	if (shouldScrollToSelection && isSelected)
	{
		ImGui::SetScrollHereY(0.5f);
		shouldScrollToSelection = false;
	}

	DrawContextMenu(browser, entry, key, selectedKey, intents);
	DrawFavouriteToggle(entry, start, key, intents);

	ImGui::TableNextColumn();

	if (entry.state == Browser::EServerState::Online && entry.summary.hasPassword)
	{
		DrawText(LKT_ICON_LOCK, GetThemeColors().textDisabled);
	}

	ImGui::TableNextColumn();
	DrawNameCell(entry);
	DrawDetailCells(entry);
	ImGui::TableNextColumn();
	DrawCountryCell(entry);
	ImGui::PopID();

	return isDoubleClicked;
}

//////////////////////////////////////////////////////////////////////////
std::string_view DescribeEmpty(Browser::SGameStatus const& status, size_t numEntries)
{
	std::string_view text{ "No server matches the filter" };

	if (numEntries == 0 && (status.isRefreshing || !status.hasRefreshed))
	{
		text = "Asking the masters…";
	}
	else if (numEntries == 0)
	{
		text = "No servers";
	}

	return text;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void DrawServerTable(Browser::CBrowser const& browser, uint64_t& selectedKey, bool& shouldScrollToSelection, SFrameIntents& intents)
{
	Query::EGame const game{ browser.GetSelectedGame() };
	Config::SSortOrder const& saved{ browser.GetSettings().games[static_cast<size_t>(game)].sort };
	ImGuiTableFlags const flags{ ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV };
	bool isJoinWanted{ false };

	ImGui::PushID(static_cast<int>(game));

	if (ImGui::BeginTable("##servers", NumColumns, flags, ImGui::GetContentRegionAvail()))
	{
		// Every frame, so the routes stay registered; focused routing keeps Enter in the search field from joining.
		bool const isEnterPressed{ ImGui::Shortcut(ImGuiKey_Enter) };
		bool const isKeypadEnterPressed{ ImGui::Shortcut(ImGuiKey_KeypadEnter) };

		SetupColumns(saved);
		ReadSortSpecs(saved, intents);

		std::span<uint32_t const> const rows{ browser.GetRows() };
		std::span<Browser::SServerEntry const> const entries{ browser.GetEntries() };
		ImGuiListClipper clipper{};

		clipper.Begin(static_cast<int>(rows.size()));

		if (shouldScrollToSelection)
		{
			auto const selected{ std::ranges::find_if(rows, [entries, selectedKey](uint32_t row) { return Query::ToKey(entries[row].address) == selectedKey; }) };

			if (selected != rows.end())
			{
				clipper.IncludeItemByIndex(static_cast<int>(selected - rows.begin()));
			}
			else
			{
				shouldScrollToSelection = false;
			}
		}

		while (clipper.Step())
		{
			for (int index{ clipper.DisplayStart }; index < clipper.DisplayEnd; ++index)
			{
				isJoinWanted = DrawRow(browser, entries[rows[static_cast<size_t>(index)]], selectedKey, shouldScrollToSelection, intents) || isJoinWanted;
			}
		}

		if (rows.empty())
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(NameColumn);
			DrawText(DescribeEmpty(browser.GetStatus(game), entries.size()), GetThemeColors().textDisabled);
		}

		isJoinWanted = ((isEnterPressed || isKeypadEnterPressed) && selectedKey != NoSelection) || isJoinWanted;
		ImGui::EndTable();
	}

	if (isJoinWanted && CountUsableLaunchers(browser) > 1)
	{
		OpenJoinWithPopup();
	}
	else if (isJoinWanted)
	{
		intents.action = SServerAction{ EServerAction::Join, selectedKey };
	}

	DrawJoinWithPopup(browser, selectedKey, intents);
	ImGui::PopID();
}
} // namespace Lkt::Ui
