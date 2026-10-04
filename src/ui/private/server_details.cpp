#include "server_details.hpp"
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
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace Lkt::Ui
{
namespace
{
// A script decides how many fields players have; the table stays readable, and within ImGui's column limit.
constexpr size_t MaxFieldColumns{ 8 };

//////////////////////////////////////////////////////////////////////////
void DrawDisabledText(std::string_view text)
{
	ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().textDisabled);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopStyleColor();
}

//////////////////////////////////////////////////////////////////////////
void DrawClippedDisabledText(std::string_view text)
{
	float const available{ ImGui::GetContentRegionAvail().x };

	DrawDisabledText(text);

	if (ImGui::GetItemRectSize().x > available && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%.*s", static_cast<int>(text.size()), text.data());
	}
}

//////////////////////////////////////////////////////////////////////////
void SameLineIfButtonFits(std::string_view label, float rowEnd)
{
	ImGuiStyle const& style{ ImGui::GetStyle() };
	float const width{ ImGui::CalcTextSize(label.data(), label.data() + label.size(), true).x + style.FramePadding.x * 2.0f };

	if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + width <= rowEnd)
	{
		ImGui::SameLine();
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawSummary(Browser::SServerEntry const& entry, uint64_t key, SFrameIntents& intents)
{
	std::array<char, Query::MaxFormattedAddress> address{};
	std::array<char, 160> buffer{};
	Query::SServerSummary const& summary{ entry.summary };
	std::string_view const formattedAddress{ Query::FormatAddressTo(entry.joinAddress, address) };

	ImGui::PushTextWrapPos(0.0f);

	if (entry.state == Browser::EServerState::Online && !summary.name.plain.empty())
	{
		DrawWrappedStyledText(summary.name);
	}
	else
	{
		ImGui::TextUnformatted(formattedAddress.data(), formattedAddress.data() + formattedAddress.size());
	}

	DrawDisabledText(formattedAddress);
	ImGui::SameLine();

	if (ImGui::SmallButton(LKT_ICON_COPY "##copy"))
	{
		intents.action = SServerAction{ EServerAction::CopyAddress, key };
	}

	ImGui::SetItemTooltip("Copy the address");

	if (entry.country != Geo::NoCountry)
	{
		gFlagAtlas.DrawItem(entry.country);
		ImGui::SameLine();
		DrawDisabledText(Geo::GetCountry(entry.country).name);
		ImGui::SetItemTooltip("IP Geolocation by DB-IP");
	}

	if (entry.state == Browser::EServerState::Online)
	{
		std::string_view const where{ summary.mod.empty()
			? FormatTo(buffer, "{} · {}", summary.map, summary.mode)
			: FormatTo(buffer, "{} · {} · {}", summary.map, summary.mode, summary.mod) };

		ImGui::TextUnformatted(where.data(), where.data() + where.size());

		std::string_view const load{ (summary.maxPlayers > 0)
			? FormatTo(buffer, "{} of {} players · {} ms", summary.numPlayers, summary.maxPlayers, entry.pingMs)
			: FormatTo(buffer, "{} {} · {} ms", summary.numPlayers, (summary.numPlayers == 1) ? "player" : "players", entry.pingMs) };

		ImGui::TextUnformatted(load.data(), load.data() + load.size());
	}

	ImGui::PopTextWrapPos();
}

//////////////////////////////////////////////////////////////////////////
// A Join that cannot work is disabled up front, with the reason, rather than failing after a click.
void DrawJoinButton(Browser::CBrowser const& browser, uint64_t key, SFrameIntents& intents)
{
	ImGui::BeginDisabled(!browser.GetJoinLauncher(browser.GetSelectedGame()).has_value());

	if (ImGui::Button(LKT_ICON_PLAY " Join"))
	{
		intents.action = SServerAction{ EServerAction::Join, key };
	}

	ImGui::EndDisabled();
	DrawJoinTooltip(browser);
}

//////////////////////////////////////////////////////////////////////////
void DrawButtons(Browser::CBrowser const& browser, Browser::SServerEntry const& entry, uint64_t key, SFrameIntents& intents)
{
	std::string_view const favouriteLabel{ entry.isFavourite ? LKT_ICON_STAR " Unfavourite###favourite" : LKT_ICON_STAR " Favourite###favourite" };
	float const rowEnd{ ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x };

	if (CountUsableLaunchers(browser) > 1)
	{
		if (ImGui::Button(LKT_ICON_PLAY " Join with " LKT_ICON_CARET_DOWN))
		{
			OpenJoinWithPopup();
		}

		DrawJoinWithPopup(browser, key, intents);
	}
	else
	{
		DrawJoinButton(browser, key, intents);
	}

	SameLineIfButtonFits(LKT_ICON_ROTATE " Refresh", rowEnd);

	if (ImGui::Button(LKT_ICON_ROTATE " Refresh"))
	{
		intents.action = SServerAction{ EServerAction::Refresh, key };
	}

	SameLineIfButtonFits(favouriteLabel, rowEnd);

	if (ImGui::Button(favouriteLabel.data()))
	{
		intents.action = SServerAction{ EServerAction::ToggleFavourite, key };
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawTextCell(std::string_view text)
{
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

//////////////////////////////////////////////////////////////////////////
// Score and ping get a column only when some player has one; each field a protocol reports gets its own.
void DrawPlayers(Browser::SServerEntry const& entry)
{
	std::array<char, 16> buffer{};
	float const em{ ImGui::GetFontSize() };
	std::span<Query::SPlayer const> const players{ entry.reply.players };
	bool const hasScore{ std::ranges::any_of(players, [](Query::SPlayer const& player) { return player.score.has_value(); }) };
	bool const hasPing{ std::ranges::any_of(players, [](Query::SPlayer const& player) { return player.ping.has_value(); }) };
	size_t const numFields{ std::min(entry.playerFieldKeys.size(), MaxFieldColumns) };
	int const numColumns{ 1 + (hasScore ? 1 : 0) + (hasPing ? 1 : 0) + static_cast<int>(numFields) };

	if (ImGui::BeginTable("##players", numColumns, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthStretch);

		if (hasScore)
		{
			ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, em * 3.0f);
		}

		if (hasPing)
		{
			ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, em * 3.0f);
		}

		for (size_t index{ 0 }; index < numFields; ++index)
		{
			ImGui::TableSetupColumn(entry.playerFieldKeys[index].c_str(), ImGuiTableColumnFlags_WidthFixed, em * 4.0f);
		}

		ImGui::TableHeadersRow();

		for (size_t index{ 0 }; index < players.size() && index < entry.playerNames.size(); ++index)
		{
			Query::SPlayer const& player{ players[index] };

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			DrawClippedStyledText(entry.playerNames[index]);

			if (hasScore)
			{
				DrawTextCell(player.score.has_value() ? FormatTo(buffer, "{}", *player.score) : std::string_view{});
			}

			if (hasPing)
			{
				DrawTextCell(player.ping.has_value() ? FormatTo(buffer, "{}", *player.ping) : std::string_view{});
			}

			for (size_t field{ 0 }; field < numFields; ++field)
			{
				auto const it{ std::ranges::find(player.fields, entry.playerFieldKeys[field], &Query::SRule::key) };

				DrawTextCell((it != player.fields.end()) ? std::string_view{ it->value } : std::string_view{});
			}
		}

		ImGui::EndTable();
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawRules(Browser::SServerEntry const& entry)
{
	if (ImGui::TreeNodeEx("Rules", ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		if (ImGui::BeginTable("##rules", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings))
		{
			for (Query::SRule const& rule : entry.plainRules)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				DrawClippedDisabledText(rule.key);
				ImGui::TableNextColumn();
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextUnformatted(rule.value.data(), rule.value.data() + rule.value.size());
				ImGui::PopTextWrapPos();
			}

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void DrawServerDetails(Browser::CBrowser const& browser, uint64_t selectedKey, SFrameIntents& intents)
{
	Browser::SServerEntry const* const pEntry{ (selectedKey != NoSelection) ? browser.FindEntry(selectedKey) : nullptr };

	if (pEntry != nullptr)
	{
		DrawSummary(*pEntry, selectedKey, intents);
		DrawButtons(browser, *pEntry, selectedKey, intents);
		ImGui::Separator();

		if (pEntry->state == Browser::EServerState::Online)
		{
			DrawPlayers(*pEntry);
			DrawRules(*pEntry);
		}
	}
	else
	{
		DrawDisabledText("Select a server to see who is playing.");
	}
}
} // namespace Lkt::Ui
