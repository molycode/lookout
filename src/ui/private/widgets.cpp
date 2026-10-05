#include "widgets.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cfloat>

namespace Lkt::Ui
{
namespace
{
//////////////////////////////////////////////////////////////////////////
void DrawLauncherLabelAt(ImVec2 position, std::string_view name, std::string_view location)
{
	SThemeColors const& colors{ GetThemeColors() };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };

	pDrawList->AddText(position, ImGui::GetColorU32(colors.text), name.data(), name.data() + name.size());

	if (!location.empty())
	{
		position.x += ImGui::CalcTextSize(name.data(), name.data() + name.size()).x;
		pDrawList->AddText(position, ImGui::GetColorU32(colors.textDisabled), " · ");
		position.x += ImGui::CalcTextSize(" · ").x;
		pDrawList->AddText(position, ImGui::GetColorU32(colors.textDisabled), location.data(), location.data() + location.size());
	}
}

//////////////////////////////////////////////////////////////////////////
float CalcLauncherLabelWidth(std::string_view name, std::string_view location)
{
	return ImGui::CalcTextSize(name.data(), name.data() + name.size()).x
		+ (location.empty() ? 0.0f : ImGui::CalcTextSize(" · ").x + ImGui::CalcTextSize(location.data(), location.data() + location.size()).x);
}

//////////////////////////////////////////////////////////////////////////
// Its background reaches back under the field's rounded right corners, so the two read as one frame.
bool ClearSearchButton()
{
	SThemeColors const& colors{ GetThemeColors() };
	float const size{ ImGui::GetFrameHeight() };
	float const rounding{ ImGui::GetStyle().FrameRounding };

	ImGui::SameLine(0.0f, 0.0f);

	bool const isPressed{ ImGui::InvisibleButton("##clear-search", ImVec2{ size, size }) };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };
	ImVec2 const min{ ImGui::GetItemRectMin() };
	ImVec2 const max{ ImGui::GetItemRectMax() };
	ImVec2 const glyphSize{ ImGui::CalcTextSize(LKT_ICON_XMARK) };

	pDrawList->AddRectFilled(ImVec2{ min.x - rounding, min.y }, max, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding, ImDrawFlags_RoundCornersRight);
	pDrawList->AddText(ImVec2{ min.x + (size - glyphSize.x) * 0.5f, min.y + (size - glyphSize.y) * 0.5f },
		ImGui::GetColorU32(ImGui::IsItemHovered() ? colors.text : colors.textDisabled), LKT_ICON_XMARK);
	ImGui::SetItemTooltip("Clear the search");

	return isPressed;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool SelectableText(std::string_view text, bool isSelected)
{
	ImVec2 const position{ ImGui::GetCursorScreenPos() };
	float const width{ ImGui::CalcTextSize(text.data(), text.data() + text.size()).x };
	bool const isPressed{ ImGui::Selectable("##text", isSelected, ImGuiSelectableFlags_SpanAvailWidth, ImVec2{ width, 0.0f }) };

	ImGui::GetWindowDrawList()->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), text.data(), text.data() + text.size());

	return isPressed;
}

//////////////////////////////////////////////////////////////////////////
// GetColorU32 applies the style's alpha, which BeginDisabled lowers.
bool IconButton(char const* id, std::string_view glyph)
{
	SThemeColors const& colors{ GetThemeColors() };
	float const size{ ImGui::GetTextLineHeight() };
	bool const isPressed{ ImGui::InvisibleButton(id, ImVec2{ size, size }, ImGuiButtonFlags_EnableNav) };
	ImVec2 const min{ ImGui::GetItemRectMin() };
	ImVec2 const glyphSize{ ImGui::CalcTextSize(glyph.data(), glyph.data() + glyph.size()) };

	ImGui::GetWindowDrawList()->AddText(ImVec2{ min.x + (size - glyphSize.x) * 0.5f, min.y + (size - glyphSize.y) * 0.5f },
		ImGui::GetColorU32(ImGui::IsItemHovered() ? colors.text : colors.textDisabled), glyph.data(), glyph.data() + glyph.size());

	return isPressed;
}

//////////////////////////////////////////////////////////////////////////
bool SearchField(char const* id, char const* hint, std::string& text, bool& shouldFocus)
{
	bool const hasText{ !text.empty() };

	if (shouldFocus)
	{
		ImGui::SetKeyboardFocusHere();
		shouldFocus = false;
	}

	ImGui::SetNextItemWidth(hasText ? -ImGui::GetFrameHeight() : -FLT_MIN);

	bool isChanged{ ImGui::InputTextWithHint(id, hint, &text, ImGuiInputTextFlags_EscapeClearsAll) };

	if (hasText && ClearSearchButton())
	{
		text.clear();
		shouldFocus = true;
		isChanged = true;
	}

	return isChanged;
}

//////////////////////////////////////////////////////////////////////////
float DrawEllipsised(std::string_view text, ImVec2 const& position, float maxX, ImVec4 const& color)
{
	float const width{ ImGui::CalcTextSize(text.data(), text.data() + text.size()).x };

	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), position, ImVec2{ maxX, position.y + ImGui::GetTextLineHeight() }, maxX,
		text.data(), text.data() + text.size(), nullptr);
	ImGui::PopStyleColor();

	return std::min(position.x + width, maxX);
}

//////////////////////////////////////////////////////////////////////////
bool SelectableLauncher(std::string_view name, std::string_view location)
{
	ImVec2 const position{ ImGui::GetCursorScreenPos() };
	bool const isPressed{ ImGui::Selectable("##launcher", false, ImGuiSelectableFlags_SpanAvailWidth, ImVec2{ CalcLauncherLabelWidth(name, location), 0.0f }) };

	DrawLauncherLabelAt(position, name, location);

	return isPressed;
}

//////////////////////////////////////////////////////////////////////////
void DrawLauncherLabel(std::string_view name, std::string_view location)
{
	ImVec2 const position{ ImGui::GetCursorScreenPos() };

	ImGui::Dummy(ImVec2{ CalcLauncherLabelWidth(name, location), ImGui::GetTextLineHeight() });
	DrawLauncherLabelAt(position, name, location);
}
} // namespace Lkt::Ui
