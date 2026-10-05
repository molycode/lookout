#include "game_form_view.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "games/field_problem.hpp"
#include "games/game_fields.hpp"
#include "games/game_form.hpp"
#include "query/game_catalog.hpp"
#include "query/protocol_definition.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cfloat>
#include <charconv>
#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

namespace Lkt::Ui
{
namespace
{
using Games::EGameFieldKind;
using Games::SFieldProblem;
using Games::SGameField;
using Games::SGameFormNode;

constexpr float LabelWidthEm{ 13.0f };
constexpr float HelpWidthEm{ 26.0f };
constexpr float NumberWidthEm{ 6.0f };
constexpr float MinListLines{ 2.0f };
constexpr float MaxListLines{ 8.0f };
constexpr char const* RequiredHint{ "required" };
constexpr char const* OptionalHint{ "optional" };
constexpr std::string_view ListSuffix{ "[]" };
constexpr std::string_view PaletteField{ "palette" };
constexpr std::string_view HexCodesField{ "hexCodes" };
constexpr std::string_view CodesField{ "codes" };
constexpr std::string_view RgbCodes{ "rgb" };
constexpr std::string_view ProtocolField{ "protocol" };
constexpr std::string_view PathSeparator{ " › " };

//////////////////////////////////////////////////////////////////////////
std::string JoinPath(std::string_view parent, std::string_view name)
{
	return parent.empty() ? std::string{ name } : std::format("{}.{}", parent, name);
}

//////////////////////////////////////////////////////////////////////////
SGameField const* FindField(std::string_view parent, std::string_view name)
{
	std::span<SGameField const> const fields{ Games::GetGameFields() };
	auto const it{ std::ranges::find_if(fields, [parent, name](SGameField const& field) { return field.parent == parent && field.name == name; }) };

	return (it != fields.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
std::string ToSchemaPath(SGameField const& field)
{
	std::string path{ JoinPath(field.parent, field.name) };

	if (field.kind == EGameFieldKind::GroupList)
	{
		path += ListSuffix;
	}

	return path;
}

//////////////////////////////////////////////////////////////////////////
bool IsMissing(SGameField const& field, SGameFormNode const& node)
{
	bool isMissing{ false };

	switch (field.kind)
	{
		case EGameFieldKind::TextList:
			isMissing = field.isRequired && node.texts.empty();
			break;
		case EGameFieldKind::GroupList:
			isMissing = field.isRequired && node.children.empty();
			break;
		case EGameFieldKind::Text:
		case EGameFieldKind::Characters:
		case EGameFieldKind::Number:
		case EGameFieldKind::Choice:
		case EGameFieldKind::Protocol:
			isMissing = field.isRequired && node.text.empty();
			break;
		case EGameFieldKind::Version:
		case EGameFieldKind::ProtocolOptions:
		case EGameFieldKind::Group:
			break;
	}

	return isMissing;
}

//////////////////////////////////////////////////////////////////////////
bool IsChanged(SGameFormNode const& node, SGameFormNode const* pBase)
{
	return pBase != nullptr && node != *pBase;
}

//////////////////////////////////////////////////////////////////////////
bool IsProblemAt(SFieldProblem const* pProblem, std::string_view path)
{
	return pProblem != nullptr && !path.empty() && pProblem->path == path;
}

//////////////////////////////////////////////////////////////////////////
// The field's own problem, or one of a row or line of it.
bool IsProblemWithin(SFieldProblem const* pProblem, std::string_view path)
{
	std::string_view const problemPath{ (pProblem != nullptr) ? std::string_view{ pProblem->path } : std::string_view{} };
	std::string_view const rest{ problemPath.starts_with(path) ? problemPath.substr(path.size()) : std::string_view{ "-" } };

	return !path.empty() && (rest.empty() || rest.front() == '[' || rest.front() == '.');
}

//////////////////////////////////////////////////////////////////////////
float GetButtonsWidth()
{
	return (ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemInnerSpacing.x) * 2.0f;
}

//////////////////////////////////////////////////////////////////////////
float GetControlX()
{
	return ImGui::GetFontSize() * LabelWidthEm;
}

//////////////////////////////////////////////////////////////////////////
// An icon is a text line high, so beside a framed control it moves down by the frame's padding to sit on its middle.
void AlignIconToFrame()
{
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
}

//////////////////////////////////////////////////////////////////////////
void DrawLabel(std::string_view label, bool isMissing, bool isChanged)
{
	SThemeColors const& colors{ GetThemeColors() };

	ImGui::AlignTextToFramePadding();
	ImGui::PushStyleColor(ImGuiCol_Text, isMissing ? colors.error : (isChanged ? colors.amber : colors.text));
	ImGui::TextUnformatted(label.data(), label.data() + label.size());
	ImGui::PopStyleColor();
	ImGui::SameLine(GetControlX());
	ImGui::SetNextItemWidth(-GetButtonsWidth());
}

//////////////////////////////////////////////////////////////////////////
void DrawHelpText(std::string_view text)
{
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

//////////////////////////////////////////////////////////////////////////
// A list's help also names the fields of each row.
void DrawHelpButton(std::string_view description, EGameFieldKind kind, std::string_view rowSchema)
{
	AlignIconToFrame();

	if (IconButton("##help", LKT_ICON_QUESTION))
	{
		ImGui::OpenPopup("##help");
	}

	if (ImGui::BeginPopup("##help"))
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * HelpWidthEm);
		DrawHelpText(description);

		if (kind == EGameFieldKind::Characters)
		{
			DrawHelpText("Type \\u001b for ESC and \\\\ for a backslash.");
		}
		else if (kind == EGameFieldKind::TextList)
		{
			DrawHelpText("One per line.");
		}

		for (SGameField const& field : Games::GetGameFields())
		{
			if (!rowSchema.empty() && field.parent == rowSchema)
			{
				ImGui::Spacing();
				ImGui::TextColored(GetThemeColors().amber, "%.*s", static_cast<int>(field.label.size()), field.label.data());
				DrawHelpText(field.description);
			}
		}

		ImGui::PopTextWrapPos();
		ImGui::EndPopup();
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawHelp(std::string_view description, EGameFieldKind kind, std::string_view rowSchema)
{
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
	DrawHelpButton(description, kind, rowSchema);
}

//////////////////////////////////////////////////////////////////////////
bool DrawReset(SGameFormNode& node, SGameFormNode const* pBase)
{
	bool isReset{ false };

	if (IsChanged(node, pBase))
	{
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		AlignIconToFrame();

		if (IconButton("##reset", LKT_ICON_ROTATE_LEFT))
		{
			node = *pBase;
			isReset = true;
		}

		ImGui::SetItemTooltip("Back to the download");
	}

	return isReset;
}

//////////////////////////////////////////////////////////////////////////
void DrawUnder(std::string_view text, ImVec4 const& color)
{
	ImGui::SetCursorPosX(GetControlX());
	ImGui::PushTextWrapPos(0.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopStyleColor();
	ImGui::PopTextWrapPos();
}

//////////////////////////////////////////////////////////////////////////
void DrawNote(SGameFormNode const& node)
{
	if (!node.note.empty())
	{
		DrawUnder(node.note, GetThemeColors().textDisabled);
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawProblem(SFieldProblem const* pProblem, std::string_view path, bool isWithin)
{
	if (isWithin ? IsProblemWithin(pProblem, path) : IsProblemAt(pProblem, path))
	{
		DrawUnder(isWithin ? DescribeFieldProblem(*pProblem) : std::string{ pProblem->reason }, GetThemeColors().error);
	}
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::string> SplitLines(std::string_view text)
{
	std::vector<std::string> lines{};

	for (auto const line : text | std::views::split('\n'))
	{
		lines.emplace_back(line.begin(), line.end());
	}

	// A newline typed at the end starts no entry yet.
	if (!lines.empty() && lines.back().empty())
	{
		lines.pop_back();
	}

	return lines;
}

//////////////////////////////////////////////////////////////////////////
bool DrawScalar(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, std::string_view path, SFieldProblem const* pProblem)
{
	ImGuiInputTextFlags const flags{ (field.kind == EGameFieldKind::Number) ? ImGuiInputTextFlags_CharsDecimal : ImGuiInputTextFlags_None };

	DrawLabel(field.label, IsMissing(field, node), IsChanged(node, pBase));

	bool isEdited{ ImGui::InputTextWithHint("##value", field.isRequired ? RequiredHint : OptionalHint, &node.text, flags) };

	DrawHelp(field.description, field.kind, {});
	isEdited = DrawReset(node, pBase) || isEdited;
	DrawNote(node);
	DrawProblem(pProblem, path, false);

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// A value not among the names, such as a protocol that did not load, is shown as it is.
template<typename TNames>
bool DrawChoice(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, TNames&& names, std::string_view path, SFieldProblem const* pProblem)
{
	bool isEdited{ false };

	DrawLabel(field.label, IsMissing(field, node), IsChanged(node, pBase));

	if (ImGui::BeginCombo("##value", node.text.c_str()))
	{
		for (std::string_view const name : names)
		{
			if (ImGui::Selectable(name.data(), name == node.text))
			{
				node.text = name;
				isEdited = true;
			}
		}

		ImGui::EndCombo();
	}

	DrawHelp(field.description, field.kind, {});
	isEdited = DrawReset(node, pBase) || isEdited;
	DrawNote(node);
	DrawProblem(pProblem, path, false);

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
bool DrawTextList(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, std::string_view path, SFieldProblem const* pProblem)
{
	std::string text{};

	for (size_t index{ 0 }; index < node.texts.size(); ++index)
	{
		text += (index == 0) ? node.texts[index] : std::format("\n{}", node.texts[index]);
	}

	float const lines{ std::clamp(static_cast<float>(node.texts.size() + 1), MinListLines, MaxListLines) };
	float const height{ lines * ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f };

	DrawLabel(field.label, IsMissing(field, node), IsChanged(node, pBase));

	bool isEdited{ ImGui::InputTextMultiline("##value", &text, ImVec2{ -GetButtonsWidth(), height }) };

	// Emptied by the user, an optional list leaves the description; only one the file wrote as [] keeps it.
	if (isEdited)
	{
		node.texts = SplitLines(text);
		node.isPresent = !node.texts.empty();
	}

	DrawHelp(field.description, field.kind, {});
	isEdited = DrawReset(node, pBase) || isEdited;
	DrawNote(node);
	DrawProblem(pProblem, path, true);

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
bool DrawFields(std::string_view schemaParent, SGameFormNode& group, SGameFormNode const* pBaseGroup, std::string_view path, SFieldProblem const* pProblem);

//////////////////////////////////////////////////////////////////////////
bool DrawGroup(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, std::string_view path, SFieldProblem const* pProblem)
{
	bool isEdited{ false };

	ImGui::Spacing();
	DrawLabel(field.label, false, IsChanged(node, pBase));

	if (field.isRequired)
	{
		ImGui::Dummy(ImVec2{ 0.0f, ImGui::GetFrameHeight() });
	}
	else
	{
		isEdited = ImGui::Checkbox("##present", &node.isPresent);
	}

	DrawHelp(field.description, field.kind, {});
	isEdited = DrawReset(node, pBase) || isEdited;
	DrawNote(node);

	if (field.isRequired || node.isPresent)
	{
		ImGui::Indent();
		isEdited = DrawFields(ToSchemaPath(field), node, pBase, path, pProblem) || isEdited;
		ImGui::Unindent();
	}

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// Each row is applied after the table, so no field a row's controls are bound to moves while they are drawn.
bool DrawGroupList(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, std::string_view path, SFieldProblem const* pProblem)
{
	std::string const schema{ ToSchemaPath(field) };
	std::span<SGameField const> const fields{ Games::GetGameFields() };
	int const numColumns{ static_cast<int>(std::ranges::count(fields, std::string_view{ schema }, &SGameField::parent)) };
	std::optional<size_t> removed{};
	bool isEdited{ false };

	DrawNote(node);

	if (!node.children.empty() && ImGui::BeginTable("##rows", numColumns + 1, ImGuiTableFlags_SizingStretchProp))
	{
		for (SGameField const& column : fields)
		{
			if (column.parent == schema)
			{
				bool const isNumber{ column.kind == EGameFieldKind::Number };

				ImGui::TableSetupColumn(column.label.data(), isNumber ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch,
					isNumber ? ImGui::GetFontSize() * NumberWidthEm : 0.0f);
			}
		}

		ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());
		ImGui::TableHeadersRow();

		for (size_t index{ 0 }; index < node.children.size(); ++index)
		{
			SGameFormNode& row{ node.children[index] };

			ImGui::PushID(static_cast<int>(index));
			ImGui::TableNextRow();

			for (SGameField const& column : fields)
			{
				if (column.parent == schema)
				{
					ImGuiInputTextFlags const flags{ (column.kind == EGameFieldKind::Number) ? ImGuiInputTextFlags_CharsDecimal : ImGuiInputTextFlags_None };

					ImGui::TableNextColumn();
					ImGui::PushID(column.name.data(), column.name.data() + column.name.size());
					ImGui::SetNextItemWidth(-FLT_MIN);
					isEdited = ImGui::InputTextWithHint("##value", column.isRequired ? RequiredHint : OptionalHint, &Games::GetFormField(row, column.name).text,
						flags) || isEdited;
					ImGui::PopID();
				}
			}

			ImGui::TableNextColumn();
			AlignIconToFrame();

			if (IconButton("##remove", LKT_ICON_TRASH))
			{
				removed = index;
			}

			ImGui::SetItemTooltip("Remove this row");
			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (removed.has_value())
	{
		node.children.erase(node.children.begin() + static_cast<std::ptrdiff_t>(*removed));
		node.isPresent = !node.children.empty();
		isEdited = true;
	}

	if (ImGui::Button(LKT_ICON_PLUS " Add"))
	{
		node.children.emplace_back(Games::MakeGameFormItem(JoinPath(field.parent, field.name)));
		node.isPresent = true;
		isEdited = true;
	}

	if (IsChanged(node, pBase))
	{
		ImGui::SameLine();

		if (ImGui::Button(LKT_ICON_ROTATE_LEFT " Back to the download"))
		{
			node = *pBase;
			isEdited = true;
		}
	}

	DrawProblem(pProblem, path, true);

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// The selected protocol's options, then any other the description holds, which the check names and which can only go.
bool DrawProtocolOptions(SGameField const& field, SGameFormNode& parent, SGameFormNode const* pBaseParent, std::string_view parentPath,
	SFieldProblem const* pProblem)
{
	SGameFormNode const* const pProtocolNode{ Games::FindFormField(parent, ProtocolField) };
	std::string const protocolName{ (pProtocolNode != nullptr) ? pProtocolNode->text : std::string{} };
	std::span<Query::SProtocolDefinition const> const protocols{ Query::GetProtocolCatalog() };
	auto const protocol{ std::ranges::find(protocols, protocolName, &Query::SProtocolDefinition::name) };
	std::string const path{ JoinPath(parentPath, field.name) };
	SGameFormNode const* const pBaseOptions{ (pBaseParent != nullptr) ? Games::FindFormField(*pBaseParent, field.name) : nullptr };
	SGameFormNode& options{ Games::GetFormField(parent, field.name) };
	bool isEdited{ false };

	ImGui::Indent();

	if (protocol != protocols.end())
	{
		for (Query::SProtocolOption const& option : protocol->options)
		{
			SGameFormNode& value{ Games::GetFormField(options, option.name) };
			SGameFormNode absent{};
			SGameFormNode const* pBase{ (pBaseOptions != nullptr) ? Games::FindFormField(*pBaseOptions, option.name) : nullptr };

			absent.name = option.name;
			pBase = (pBaseOptions != nullptr && pBase == nullptr) ? &absent : pBase;
			ImGui::PushID(option.name.c_str());
			DrawLabel(option.name, option.isRequired && value.text.empty(), IsChanged(value, pBase));
			isEdited = ImGui::InputTextWithHint("##value", option.isRequired ? RequiredHint : OptionalHint, &value.text) || isEdited;
			DrawHelp(option.description, EGameFieldKind::Text, {});
			isEdited = DrawReset(value, pBase) || isEdited;
			DrawNote(value);
			DrawProblem(pProblem, JoinPath(path, option.name), false);
			ImGui::PopID();
		}
	}

	for (SGameFormNode& value : options.children)
	{
		bool const isDeclared{ protocol != protocols.end() && std::ranges::contains(protocol->options, value.name, &Query::SProtocolOption::name) };

		if (!isDeclared && !value.text.empty())
		{
			ImGui::PushID(value.name.c_str());
			DrawLabel(value.name, true, false);
			isEdited = ImGui::InputText("##value", &value.text) || isEdited;
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			AlignIconToFrame();

			if (IconButton("##remove", LKT_ICON_TRASH))
			{
				value.text.clear();
				isEdited = true;
			}

			ImGui::SetItemTooltip("Remove this option");
			DrawProblem(pProblem, JoinPath(path, value.name), false);
			ImGui::PopID();
		}
	}

	ImGui::Unindent();

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
bool DrawField(SGameField const& field, SGameFormNode& parent, SGameFormNode const* pBaseParent, std::string_view parentPath, SFieldProblem const* pProblem)
{
	std::string const path{ JoinPath(parentPath, field.name) };
	SGameFormNode const* const pBase{ (pBaseParent != nullptr) ? Games::FindFormField(*pBaseParent, field.name) : nullptr };
	bool isEdited{ false };

	ImGui::PushID(field.name.data(), field.name.data() + field.name.size());

	switch (field.kind)
	{
		case EGameFieldKind::Text:
		case EGameFieldKind::Characters:
		case EGameFieldKind::Number:
			isEdited = DrawScalar(field, Games::GetFormField(parent, field.name), pBase, path, pProblem);
			break;
		case EGameFieldKind::Choice:
			isEdited = DrawChoice(field, Games::GetFormField(parent, field.name), pBase, field.choices, path, pProblem);
			break;
		case EGameFieldKind::Protocol:
			isEdited = DrawChoice(field, Games::GetFormField(parent, field.name), pBase,
				Query::GetProtocolCatalog() | std::views::transform([](Query::SProtocolDefinition const& protocol) { return std::string_view{ protocol.name }; }),
				path, pProblem);
			break;
		case EGameFieldKind::ProtocolOptions:
			isEdited = DrawProtocolOptions(field, parent, pBaseParent, parentPath, pProblem);
			break;
		case EGameFieldKind::TextList:
			isEdited = DrawTextList(field, Games::GetFormField(parent, field.name), pBase, path, pProblem);
			break;
		case EGameFieldKind::Group:
			isEdited = DrawGroup(field, Games::GetFormField(parent, field.name), pBase, path, pProblem);
			break;
		case EGameFieldKind::GroupList:
			isEdited = DrawGroupList(field, Games::GetFormField(parent, field.name), pBase, path, pProblem);
			break;
		case EGameFieldKind::Version:
			break;
	}

	ImGui::PopID();

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// Rgb codes carry their colour, so the palette and hex codes are hidden for them; they stay in the form, unwritten.
bool DrawFields(std::string_view schemaParent, SGameFormNode& group, SGameFormNode const* pBaseGroup, std::string_view path, SFieldProblem const* pProblem)
{
	SGameFormNode const* const pCodes{ Games::FindFormField(group, CodesField) };
	bool const isRgb{ pCodes != nullptr && pCodes->text == RgbCodes };
	bool isEdited{ false };

	for (SGameField const& field : Games::GetGameFields())
	{
		if (field.parent == schemaParent && !(isRgb && (field.name == PaletteField || field.name == HexCodesField)))
		{
			isEdited = DrawField(field, group, pBaseGroup, path, pProblem) || isEdited;
		}
	}

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
bool IsSection(SGameField const& field)
{
	return field.parent.empty() && (field.kind == EGameFieldKind::Group || field.kind == EGameFieldKind::GroupList);
}

//////////////////////////////////////////////////////////////////////////
// A section with a problem or a change shows it on its header, so a closed one still does.
bool DrawSection(SGameField const& field, SGameFormNode& form, SGameFormNode const* pDownloaded, SFieldProblem const* pProblem)
{
	SThemeColors const& colors{ GetThemeColors() };
	SGameFormNode& node{ Games::GetFormField(form, field.name) };
	SGameFormNode const* const pBase{ (pDownloaded != nullptr) ? Games::FindFormField(*pDownloaded, field.name) : nullptr };
	std::string const path{ field.name };
	std::string const rowSchema{ (field.kind == EGameFieldKind::GroupList) ? ToSchemaPath(field) : std::string{} };
	bool const hasContent{ field.isRequired || node.isPresent || !node.children.empty() };
	ImGuiTreeNodeFlags const flags{ (hasContent ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None) | ImGuiTreeNodeFlags_AllowOverlap };
	float const helpX{ ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - GetButtonsWidth() };
	bool isEdited{ false };

	ImGui::PushID(field.name.data(), field.name.data() + field.name.size());
	ImGui::PushStyleColor(ImGuiCol_Text, IsProblemWithin(pProblem, path) ? colors.error : (IsChanged(node, pBase) ? colors.amber : colors.text));

	bool const isOpen{ ImGui::CollapsingHeader(field.label.data(), flags) };

	ImGui::PopStyleColor();
	ImGui::SameLine(helpX);
	DrawHelpButton(field.description, field.kind, rowSchema);

	if (isOpen && field.kind == EGameFieldKind::GroupList)
	{
		isEdited = DrawGroupList(field, node, pBase, path, pProblem);
	}
	else if (isOpen)
	{
		DrawNote(node);

		if (!field.isRequired)
		{
			isEdited = ImGui::Checkbox("Include these details", &node.isPresent);
			isEdited = DrawReset(node, pBase) || isEdited;
		}

		if (field.isRequired || node.isPresent)
		{
			isEdited = DrawFields(field.name, node, pBase, path, pProblem) || isEdited;
		}
	}

	ImGui::PopID();

	return isEdited;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool DrawGameForm(SGameFormNode& form, SGameFormNode const* pDownloaded, SFieldProblem const* pProblem)
{
	bool isEdited{ false };

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (SGameField const& field : Games::GetGameFields())
		{
			if (field.parent.empty() && !IsSection(field))
			{
				isEdited = DrawField(field, form, pDownloaded, {}, pProblem) || isEdited;
			}
		}
	}

	for (SGameField const& field : Games::GetGameFields())
	{
		if (IsSection(field))
		{
			isEdited = DrawSection(field, form, pDownloaded, pProblem) || isEdited;
		}
	}

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// In the form's words: "Masters › row 1 › Port: …" rather than "masters[0].port: …".
std::string DescribeFieldProblem(SFieldProblem const& problem)
{
	std::string described{};
	std::string schema{};
	std::string_view rest{ problem.path };
	EGameFieldKind previousKind{ EGameFieldKind::Text };

	while (!rest.empty())
	{
		if (rest.front() == '[')
		{
			size_t const end{ std::min(rest.find(']'), rest.size()) };
			size_t index{ 0 };

			std::from_chars(rest.data() + 1, rest.data() + end, index);
			described += std::format("{}{} {}", PathSeparator, (previousKind == EGameFieldKind::GroupList) ? "row" : "line", index + 1);
			schema += (previousKind == EGameFieldKind::GroupList) ? ListSuffix : std::string_view{};
			rest = rest.substr(std::min(end + 1, rest.size()));
		}
		else
		{
			rest = rest.starts_with('.') ? rest.substr(1) : rest;

			size_t const end{ rest.find_first_of(".[") };
			std::string_view const name{ rest.substr(0, end) };
			SGameField const* const pField{ FindField(schema, name) };

			described += std::format("{}{}", described.empty() ? std::string_view{} : PathSeparator, (pField != nullptr) ? pField->label : name);
			previousKind = (pField != nullptr) ? pField->kind : EGameFieldKind::Text;
			schema = JoinPath(schema, name);
			rest = (end == std::string_view::npos) ? std::string_view{} : rest.substr(end);
		}
	}

	return described.empty() ? problem.reason : std::format("{}: {}", described, problem.reason);
}
} // namespace Lkt::Ui
