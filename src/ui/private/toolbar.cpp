#include "toolbar.hpp"
#include "flag_atlas.hpp"
#include "format_to.hpp"
#include "frame_intents.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "browser/browser.hpp"
#include "browser/text_compare.hpp"
#include "geo/countries.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <array>
#include <cstdint>

namespace Lkt::Ui
{
namespace
{
constexpr std::array<uint32_t, 5> PingLimits{ Config::NoPingLimit, 50, 100, 150, 250 };
constexpr std::array<uint32_t, 5> AutoRefreshChoices{ 0, 30, 60, 120, 300 };
constexpr char const* AutoRefreshPopupId{ "##auto-refresh-menu" };

//////////////////////////////////////////////////////////////////////////
std::string_view DescribeAutoRefresh(uint32_t seconds, std::array<char, 32>& buffer)
{
	std::string_view text{};

	if (seconds == 0)
	{
		text = FormatTo(buffer, "Off");
	}
	else if (seconds % 60 == 0)
	{
		text = FormatTo(buffer, "Every {} min", seconds / 60);
	}
	else
	{
		text = FormatTo(buffer, "Every {} s", seconds);
	}

	return text;
}

//////////////////////////////////////////////////////////////////////////
void DrawRefreshButtons(Browser::CBrowser const& browser, SFrameIntents& intents)
{
	uint32_t const autoRefreshSeconds{ browser.GetSettings().autoRefreshSeconds };
	std::array<char, 32> buffer{};
	std::array<char, 96> tooltip{};

	if (ImGui::Button(browser.GetStatus(browser.GetSelectedGame()).isRefreshing ? LKT_ICON_ROTATE " Refreshing…###refresh" : LKT_ICON_ROTATE " Refresh###refresh"))
	{
		intents.refresh = true;
	}

	std::string_view const refreshTooltip{ (autoRefreshSeconds == 0) ? FormatTo(tooltip, "Refresh the server list (F5)")
		: FormatTo(tooltip, "Refresh the server list (F5)\nAuto-refresh: {}", DescribeAutoRefresh(autoRefreshSeconds, buffer)) };

	ImGui::SetItemTooltip("%.*s", static_cast<int>(refreshTooltip.size()), refreshTooltip.data());
	ImGui::SameLine(0.0f, 1.0f);

	if (ImGui::Button(LKT_ICON_CARET_DOWN "##auto-refresh"))
	{
		ImGui::OpenPopup(AutoRefreshPopupId);
	}

	ImGui::SetItemTooltip("Auto-refresh");

	if (ImGui::BeginPopup(AutoRefreshPopupId))
	{
		ImGui::TextDisabled("Auto-refresh");
		ImGui::Separator();

		for (uint32_t const seconds : AutoRefreshChoices)
		{
			if (ImGui::MenuItem(DescribeAutoRefresh(seconds, buffer).data(), nullptr, seconds == autoRefreshSeconds))
			{
				intents.autoRefreshSeconds = seconds;
			}
		}

		ImGui::EndPopup();
	}
}

//////////////////////////////////////////////////////////////////////////
std::string_view DescribePing(uint32_t maxPingMs, std::array<char, 32>& buffer)
{
	return (maxPingMs == Config::NoPingLimit) ? FormatTo(buffer, "Any ping") : FormatTo(buffer, "Ping ≤ {} ms", maxPingMs);
}

//////////////////////////////////////////////////////////////////////////
void DrawPingFilter(Config::SServerFilter const& filter, SFrameIntents& intents)
{
	std::array<char, 32> previewBuffer{};
	std::array<char, 32> labelBuffer{};

	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);

	if (ImGui::BeginCombo("##ping", DescribePing(filter.maxPingMs, previewBuffer).data()))
	{
		for (uint32_t const limit : PingLimits)
		{
			if (ImGui::Selectable(DescribePing(limit, labelBuffer).data(), limit == filter.maxPingMs))
			{
				Config::SServerFilter changed{ filter };

				changed.maxPingMs = limit;
				intents.filter = changed;
			}
		}

		ImGui::EndCombo();
	}
}

//////////////////////////////////////////////////////////////////////////
// A saved mod that no server runs at the moment still shows, so it can be cleared.
void DrawModFilter(Browser::CBrowser const& browser, Config::SServerFilter const& filter, SFrameIntents& intents)
{
	std::string_view const preview{ filter.mod.empty() ? std::string_view{ "Any mod" } : std::string_view{ filter.mod } };

	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);

	if (ImGui::BeginCombo("##mod", nullptr, ImGuiComboFlags_CustomPreview))
	{
		std::span<std::string const> const mods{ browser.GetMods() };

		if (ImGui::Selectable("Any mod", filter.mod.empty()))
		{
			Config::SServerFilter changed{ filter };

			changed.mod.clear();
			intents.filter = changed;
		}

		for (size_t index{ 0 }; index < mods.size(); ++index)
		{
			ImGui::PushID(static_cast<int>(index));

			if (SelectableText(mods[index], Browser::EqualsIgnoringCase(mods[index], filter.mod)))
			{
				Config::SServerFilter changed{ filter };

				changed.mod = mods[index];
				intents.filter = changed;
			}

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	// A preview label would end at "##" in a mod name.
	if (ImGui::BeginComboPreview())
	{
		ImGui::TextUnformatted(preview.data(), preview.data() + preview.size());
		ImGui::EndComboPreview();
	}
}

//////////////////////////////////////////////////////////////////////////
// The caller pushes an ID.
bool SelectableCountry(uint8_t country, bool isSelected)
{
	ImVec2 const position{ ImGui::GetCursorScreenPos() };
	std::string_view const name{ Geo::GetCountry(country).name };
	float const textX{ position.x + CFlagAtlas::GetWidth() + ImGui::GetStyle().ItemInnerSpacing.x };
	bool const isPressed{ ImGui::Selectable("##country", isSelected) };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };

	gFlagAtlas.Draw(pDrawList, country, position);
	pDrawList->AddText(ImVec2{ textX, position.y }, ImGui::GetColorU32(ImGuiCol_Text), name.data(), name.data() + name.size());

	return isPressed;
}

//////////////////////////////////////////////////////////////////////////
// A saved country that no server is in at the moment still shows, so it can be cleared.
void DrawCountryFilter(Browser::CBrowser const& browser, Config::SServerFilter const& filter, SFrameIntents& intents)
{
	uint8_t const selected{ Geo::FindCountryByCode(filter.country) };

	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);

	if (ImGui::BeginCombo("##country", nullptr, static_cast<ImGuiComboFlags>(ImGuiComboFlags_CustomPreview) | ImGuiComboFlags_HeightLarge))
	{
		if (ImGui::Selectable("Any country", filter.country.empty()))
		{
			Config::SServerFilter changed{ filter };

			changed.country.clear();
			intents.filter = changed;
		}

		for (uint8_t const country : browser.GetCountries())
		{
			ImGui::PushID(country);

			if (SelectableCountry(country, country == selected))
			{
				Config::SServerFilter changed{ filter };

				changed.country = Geo::GetCountry(country).code;
				intents.filter = changed;
			}

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	if (ImGui::BeginComboPreview())
	{
		if (selected != Geo::NoCountry)
		{
			std::string_view const name{ Geo::GetCountry(selected).name };

			gFlagAtlas.DrawItem(selected);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted(name.data(), name.data() + name.size());
		}
		else
		{
			ImGui::TextUnformatted(filter.country.empty() ? "Any country" : filter.country.c_str());
		}

		ImGui::EndComboPreview();
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CToolbar::Draw(Browser::CBrowser const& browser, SFrameIntents& intents)
{
	Query::EGame const game{ browser.GetSelectedGame() };
	Config::SServerFilter const& filter{ browser.GetSettings().games[static_cast<size_t>(game)].filter };

	if (!m_hasGame || game != m_game)
	{
		m_search = filter.search;
		m_game = game;
		m_hasGame = true;
	}

	DrawRefreshButtons(browser, intents);
	ImGui::SameLine();

	if (ImGui::Button(LKT_ICON_PLUS "##add-server", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }))
	{
		intents.openAddServer = true;
	}

	ImGui::SetItemTooltip("Add a server by its address (Ctrl+N)");

	bool showEmpty{ filter.showEmpty };
	bool showFull{ filter.showFull };

	ImGui::SameLine();

	if (ImGui::Checkbox("Empty", &showEmpty))
	{
		Config::SServerFilter changed{ filter };

		changed.showEmpty = showEmpty;
		intents.filter = changed;
	}

	ImGui::SameLine();

	if (ImGui::Checkbox("Full", &showFull))
	{
		Config::SServerFilter changed{ filter };

		changed.showFull = showFull;
		intents.filter = changed;
	}

	ImGui::SameLine();
	DrawPingFilter(filter, intents);
	ImGui::SameLine();
	DrawModFilter(browser, filter, intents);
	ImGui::SameLine();
	DrawCountryFilter(browser, filter, intents);
	ImGui::SameLine();

	bool const wantsSearch{ ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_RouteGlobal) };

	// Focusing skips the modal check, so under a prompt the typing would land behind it.
	if ((wantsSearch && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) || m_shouldFocusSearch)
	{
		ImGui::SetKeyboardFocusHere();
		m_shouldFocusSearch = false;
	}

	bool const hasSearch{ !m_search.empty() };

	ImGui::SetNextItemWidth(hasSearch ? -ImGui::GetFrameHeight() : -FLT_MIN);

	if (ImGui::InputTextWithHint("##search", LKT_ICON_SEARCH "  Names, maps, mods, countries, players", &m_search, ImGuiInputTextFlags_EscapeClearsAll))
	{
		Config::SServerFilter changed{ filter };

		changed.search = m_search;
		intents.filter = changed;
	}

	if (hasSearch)
	{
		DrawClearButton(filter, intents);
	}
}

//////////////////////////////////////////////////////////////////////////
// Its background reaches back under the field's rounded right corners, so the two read as one frame.
void CToolbar::DrawClearButton(Config::SServerFilter const& filter, SFrameIntents& intents)
{
	float const size{ ImGui::GetFrameHeight() };
	float const rounding{ ImGui::GetStyle().FrameRounding };

	ImGui::SameLine(0.0f, 0.0f);

	if (ImGui::InvisibleButton("##clear-search", ImVec2{ size, size }))
	{
		Config::SServerFilter changed{ filter };

		m_search.clear();
		changed.search.clear();
		intents.filter = changed;
		m_shouldFocusSearch = true;
	}

	SThemeColors const& colors{ GetThemeColors() };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };
	ImVec2 const min{ ImGui::GetItemRectMin() };
	ImVec2 const max{ ImGui::GetItemRectMax() };
	ImVec2 const glyphSize{ ImGui::CalcTextSize(LKT_ICON_XMARK) };

	pDrawList->AddRectFilled(ImVec2{ min.x - rounding, min.y }, max, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding, ImDrawFlags_RoundCornersRight);
	pDrawList->AddText(ImVec2{ min.x + (size - glyphSize.x) * 0.5f, min.y + (size - glyphSize.y) * 0.5f },
		ImGui::GetColorU32(ImGui::IsItemHovered() ? colors.text : colors.textDisabled), LKT_ICON_XMARK);
	ImGui::SetItemTooltip("Clear the search");
}
//////////////////////////////////////////////////////////////////////////
// The selected game may keep its number and yet be another, so its search is read again.
void CToolbar::OnCatalogChanged()
{
	m_hasGame = false;
}
} // namespace Lkt::Ui
