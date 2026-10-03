#include "styled_text_view.hpp"
#include "name_color.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include <tge/assert.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>

namespace Lkt::Ui
{
namespace
{
//////////////////////////////////////////////////////////////////////////
ImVec4 ToReadable(Tge::SColor const& color, ImVec4 const& text)
{
	constexpr float ByteToUnit{ 1.0f / 255.0f };

	SRgb const lifted{ LiftDarkColor(SRgb{ static_cast<float>(color.r) * ByteToUnit, static_cast<float>(color.g) * ByteToUnit,
		static_cast<float>(color.b) * ByteToUnit }, SRgb{ text.x, text.y, text.z }) };

	return ImVec4{ lifted.r, lifted.g, lifted.b, 1.0f };
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void DrawStyledText(Query::SStyledText const& text)
{
	ImVec4 const& textColor{ GetThemeColors().text };
	bool isFirst{ true };

	ImGui::BeginGroup();

	for (Query::STextRun const& run : text.runs)
	{
		if (!isFirst)
		{
			ImGui::SameLine(0.0f, 0.0f);
		}

		ImGui::PushStyleColor(ImGuiCol_Text, run.hasColor ? ToReadable(run.color, textColor) : textColor);
		ImGui::TextUnformatted(run.text.data(), run.text.data() + run.text.size());
		ImGui::PopStyleColor();
		isFirst = false;
	}

	ImGui::EndGroup();
}

//////////////////////////////////////////////////////////////////////////
void DrawClippedStyledText(Query::SStyledText const& text)
{
	float const available{ ImGui::GetContentRegionAvail().x };

	DrawStyledText(text);

	if (ImGui::GetItemRectSize().x > available && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%.*s", static_cast<int>(text.plain.size()), text.plain.data());
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawWrappedStyledText(Query::SStyledText const& text)
{
	ImFont* const pFont{ ImGui::GetFont() };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };
	ImVec4 const& textColor{ GetThemeColors().text };
	float const fontSize{ ImGui::GetFontSize() };
	float const lineHeight{ ImGui::GetTextLineHeight() };
	float const wrapWidth{ ImGui::GetContentRegionAvail().x };
	ImVec2 const origin{ ImGui::GetCursorScreenPos() };
	char const* const begin{ text.plain.data() };
	char const* const end{ begin + text.plain.size() };
	char const* lineStart{ begin };
	float y{ origin.y };
	float maxWidth{ 0.0f };

	while (lineStart < end)
	{
		char const* const lineEnd{ pFont->CalcWordWrapPosition(fontSize, lineStart, end, wrapWidth) };
		char const* runStart{ begin };
		float x{ origin.x };

		for (Query::STextRun const& run : text.runs)
		{
			char const* const runEnd{ runStart + run.text.size() };
			char const* const pieceBegin{ std::max(lineStart, runStart) };
			char const* const pieceEnd{ std::min(lineEnd, runEnd) };

			if (pieceBegin < pieceEnd)
			{
				ImU32 const color{ ImGui::GetColorU32(run.hasColor ? ToReadable(run.color, textColor) : textColor) };

				pDrawList->AddText(pFont, fontSize, ImVec2{ x, y }, color, pieceBegin, pieceEnd);
				x += pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, pieceBegin, pieceEnd).x;
			}

			runStart = runEnd;
		}

		TGE_ASSERT(runStart == end, "The runs do not make up the plain text");
		maxWidth = std::max(maxWidth, x - origin.x);
		y += lineHeight;
		lineStart = ImTextCalcWordWrapNextLineStart(lineEnd, end);
	}

	ImGui::Dummy(ImVec2{ maxWidth, std::max(y - origin.y, lineHeight) });
}
} // namespace Lkt::Ui
