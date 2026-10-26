#include "game_form_view.hpp"
#include "form_context.hpp"
#include "icons.hpp"
#include "master_listing.hpp"
#include "protocol_note.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "games/field_problem.hpp"
#include "games/game_fields.hpp"
#include "games/game_form.hpp"
#include "query/protocol_definition.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cfloat>
#include <charconv>
#include <cstddef>
#include <format>
#include <functional>
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
constexpr float SettingsWidthEm{ 34.0f };
constexpr float MinListLines{ 2.0f };
constexpr float MaxListLines{ 8.0f };
constexpr char const* RequiredHint{ "required" };
constexpr char const* OptionalHint{ "optional" };
constexpr char const* ListingHeader{ "Lists through" };
constexpr std::string_view ListSuffix{ "[]" };
constexpr std::string_view PaletteField{ "palette" };
constexpr std::string_view HexCodesField{ "hexCodes" };
constexpr std::string_view CodesField{ "codes" };
constexpr std::string_view RgbCodes{ "rgb" };
constexpr std::string_view ProtocolField{ "protocol" };
constexpr std::string_view OptionsField{ "protocolOptions" };
constexpr std::string_view PortOffsetField{ "portOffset" };
constexpr std::string_view MastersRow{ "masters[]" };
constexpr std::string_view PathSeparator{ " › " };

//////////////////////////////////////////////////////////////////////////
std::string JoinPath(std::string_view parent, std::string_view name)
{
	return parent.empty() ? std::string{ name } : std::format("{}.{}", parent, name);
}

//////////////////////////////////////////////////////////////////////////
std::string_view GetText(SGameFormNode const& node, std::string_view name)
{
	SGameFormNode const* const pField{ Games::FindFormField(node, name) };

	return (pField != nullptr) ? std::string_view{ pField->text } : std::string_view{};
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
// Nothing for a name the protocols lack: the field's problem says so.
std::string DescribeProtocolNamed(SFormContext const& context, std::string_view name)
{
	auto const protocol{ std::ranges::find(context.definitions, name, &Query::SProtocolDefinition::name) };

	return (protocol != context.definitions.end()) ? DescribeProtocol(*protocol) : std::string{};
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
// A value not among the names, such as a protocol that did not load, is shown as it is. A none label, when given, is
// the first choice and leaves the field empty; more choices, when given, follow the names.
template<typename TNames>
bool DrawChoice(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, TNames&& names, std::string_view path, SFieldProblem const* pProblem,
	std::string_view choiceNote, std::string_view noneLabel, std::function<bool(std::string&)> const& drawMoreChoices)
{
	bool isEdited{ false };

	DrawLabel(field.label, IsMissing(field, node), IsChanged(node, pBase));

	if (ImGui::BeginCombo("##value", (node.text.empty() && !noneLabel.empty()) ? noneLabel.data() : node.text.c_str()))
	{
		if (!noneLabel.empty() && ImGui::Selectable(noneLabel.data(), node.text.empty()))
		{
			node.text.clear();
			isEdited = true;
		}

		for (std::string_view const name : names)
		{
			if (ImGui::Selectable(name.data(), name == node.text))
			{
				node.text = name;
				isEdited = true;
			}
		}

		if (drawMoreChoices)
		{
			isEdited = drawMoreChoices(node.text) || isEdited;
		}

		ImGui::EndCombo();
	}

	DrawHelp(field.description, field.kind, {});
	isEdited = DrawReset(node, pBase) || isEdited;
	DrawNote(node);

	if (!choiceNote.empty())
	{
		DrawUnder(choiceNote, GetThemeColors().textDisabled);
	}

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
bool DrawFields(std::string_view schemaParent, SGameFormNode& group, SGameFormNode const* pBaseGroup, std::string_view path, SFieldProblem const* pProblem,
	SFormContext const& context);

//////////////////////////////////////////////////////////////////////////
bool DrawProtocolOptions(SGameField const& field, SGameFormNode& parent, SGameFormNode const* pBaseParent, std::string_view parentPath,
	SFieldProblem const* pProblem, SFormContext const& context);

//////////////////////////////////////////////////////////////////////////
bool DrawGroup(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, std::string_view path, SFieldProblem const* pProblem,
	SFormContext const& context)
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
		isEdited = DrawFields(ToSchemaPath(field), node, pBase, path, pProblem, context) || isEdited;
		ImGui::Unindent();
	}

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// A row's optional fields may need more than a line of text, so the table leaves them as the file has them.
bool IsCell(SGameField const& field, std::string_view schema)
{
	return field.parent == schema && field.isRequired;
}

//////////////////////////////////////////////////////////////////////////
// How the master lists, or a field behind its gear, has a problem.
bool IsSettingsProblem(SFieldProblem const* pProblem, std::string_view rowPath)
{
	return IsProblemAt(pProblem, rowPath)
		|| std::ranges::any_of(Games::GetGameFields(), [pProblem, rowPath](SGameField const& field)
		{
			return field.parent == MastersRow && !field.isRequired && IsProblemWithin(pProblem, JoinPath(rowPath, field.name));
		});
}

//////////////////////////////////////////////////////////////////////////
// A master on the game's protocol takes the game's options too, so choosing the game's takes the master's own away.
bool DrawMasterSettings(SGameFormNode& row, std::string_view rowPath, SFieldProblem const* pProblem, std::string_view inheritedProtocol,
	SFormContext const& context)
{
	SGameFormNode& protocol{ Games::GetFormField(row, ProtocolField) };
	std::string const inherited{ inheritedProtocol.empty() ? std::string{ "The game's" } : std::format("The game's ({})", inheritedProtocol) };
	bool isEdited{ false };

	ImGui::PushID(ProtocolField.data(), ProtocolField.data() + ProtocolField.size());

	if (DrawChoice(*FindField(MastersRow, ProtocolField), protocol, nullptr,
		context.definitions | std::views::transform([](Query::SProtocolDefinition const& definition) { return std::string_view{ definition.name }; }),
		JoinPath(rowPath, ProtocolField), pProblem, DescribeProtocolNamed(context, protocol.text), inherited, context.drawMoreChoices))
	{
		isEdited = true;

		if (protocol.text.empty())
		{
			SGameFormNode& options{ Games::GetFormField(row, OptionsField) };

			options.children.clear();
			options.isPresent = false;
		}
	}

	if (!protocol.text.empty() && !std::ranges::contains(context.definitions, protocol.text, &Query::SProtocolDefinition::name) && context.drawAbsent)
	{
		ImGui::SetCursorPosX(GetControlX());
		ImGui::BeginGroup();
		context.drawAbsent(protocol.text);
		ImGui::EndGroup();
	}

	ImGui::PopID();
	ImGui::PushID(OptionsField.data(), OptionsField.data() + OptionsField.size());
	isEdited = DrawProtocolOptions(*FindField(MastersRow, OptionsField), row, nullptr, rowPath, pProblem, context) || isEdited;
	ImGui::PopID();
	ImGui::PushID(PortOffsetField.data(), PortOffsetField.data() + PortOffsetField.size());
	isEdited = DrawScalar(*FindField(MastersRow, PortOffsetField), Games::GetFormField(row, PortOffsetField), nullptr, JoinPath(rowPath, PortOffsetField),
		pProblem) || isEdited;
	ImGui::PopID();
	DrawProblem(pProblem, rowPath, false);

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
bool DrawMasterGear(SGameFormNode& row, std::string_view rowPath, SFieldProblem const* pProblem, std::string_view inheritedProtocol,
	SFormContext const& context)
{
	bool isEdited{ false };

	AlignIconToFrame();

	if (IconButton("##settings", LKT_ICON_GEAR))
	{
		ImGui::OpenPopup("##settings");
	}

	ImGui::SetItemTooltip("How this master lists its servers");
	ImGui::SetNextWindowSize(ImVec2{ ImGui::GetFontSize() * SettingsWidthEm, 0.0f });

	if (ImGui::BeginPopup("##settings"))
	{
		isEdited = DrawMasterSettings(row, rowPath, pProblem, inheritedProtocol, context);
		ImGui::EndPopup();
	}

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// Each row is applied after the table, so no field a row's controls are bound to moves while they are drawn. A master
// without a protocol of its own lists through inheritedProtocol, the one of the object holding the list.
bool DrawGroupList(SGameField const& field, SGameFormNode& node, SGameFormNode const* pBase, std::string_view path, SFieldProblem const* pProblem,
	std::string_view inheritedProtocol, SFormContext const& context)
{
	std::string const schema{ ToSchemaPath(field) };
	std::span<SGameField const> const fields{ Games::GetGameFields() };
	bool const isMasters{ schema == MastersRow };
	int const numCells{ static_cast<int>(std::ranges::count_if(fields, [&schema](SGameField const& column) { return IsCell(column, schema); })) };
	int const numColumns{ numCells + (isMasters ? 2 : 0) + 1 };
	std::vector<std::string> listings{};
	float listingWidth{ ImGui::CalcTextSize(ListingHeader).x };
	std::optional<size_t> removed{};
	bool isEdited{ false };

	if (isMasters)
	{
		for (SGameFormNode const& row : node.children)
		{
			listings.emplace_back(DescribeMasterListing(inheritedProtocol, GetText(row, ProtocolField), GetText(row, PortOffsetField)));
			listingWidth = std::max(listingWidth, ImGui::CalcTextSize(listings.back().c_str()).x);
		}
	}

	DrawNote(node);

	if (!node.children.empty() && ImGui::BeginTable("##rows", numColumns, ImGuiTableFlags_SizingStretchProp))
	{
		for (SGameField const& column : fields)
		{
			if (IsCell(column, schema))
			{
				bool const isNumber{ column.kind == EGameFieldKind::Number };

				ImGui::TableSetupColumn(column.label.data(), isNumber ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch,
					isNumber ? ImGui::GetFontSize() * NumberWidthEm : 0.0f);
			}
		}

		if (isMasters)
		{
			ImGui::TableSetupColumn(ListingHeader, ImGuiTableColumnFlags_WidthFixed, listingWidth);
			ImGui::TableSetupColumn("##settings", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHeaderLabel, ImGui::GetTextLineHeight());
		}

		ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHeaderLabel, ImGui::GetTextLineHeight());
		ImGui::TableHeadersRow();

		for (size_t index{ 0 }; index < node.children.size(); ++index)
		{
			SGameFormNode& row{ node.children[index] };

			ImGui::PushID(static_cast<int>(index));
			ImGui::TableNextRow();

			for (SGameField const& column : fields)
			{
				if (IsCell(column, schema))
				{
					ImGuiInputTextFlags const flags{ (column.kind == EGameFieldKind::Number) ? ImGuiInputTextFlags_CharsDecimal : ImGuiInputTextFlags_None };

					ImGui::TableNextColumn();
					ImGui::PushID(column.name.data(), column.name.data() + column.name.size());
					ImGui::SetNextItemWidth(-FLT_MIN);
					isEdited = ImGui::InputTextWithHint("##value", RequiredHint, &Games::GetFormField(row, column.name).text, flags) || isEdited;
					ImGui::PopID();
				}
			}

			if (isMasters)
			{
				std::string const rowPath{ std::format("{}[{}]", path, index) };

				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::PushStyleColor(ImGuiCol_Text, IsSettingsProblem(pProblem, rowPath) ? GetThemeColors().error : GetThemeColors().text);
				ImGui::TextUnformatted(listings[index].c_str());
				ImGui::PopStyleColor();
				ImGui::TableNextColumn();
				isEdited = DrawMasterGear(row, rowPath, pProblem, inheritedProtocol, context) || isEdited;
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
	SFieldProblem const* pProblem, SFormContext const& context)
{
	SGameFormNode const* const pProtocolNode{ Games::FindFormField(parent, ProtocolField) };
	std::string const protocolName{ (pProtocolNode != nullptr) ? pProtocolNode->text : std::string{} };
	auto const protocol{ std::ranges::find(context.definitions, protocolName, &Query::SProtocolDefinition::name) };
	std::string const path{ JoinPath(parentPath, field.name) };
	SGameFormNode const* const pBaseOptions{ (pBaseParent != nullptr) ? Games::FindFormField(*pBaseParent, field.name) : nullptr };
	SGameFormNode& options{ Games::GetFormField(parent, field.name) };
	bool isEdited{ false };

	ImGui::Indent();

	if (protocol != context.definitions.end())
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
		bool const isDeclared{ protocol != context.definitions.end() && std::ranges::contains(protocol->options, value.name, &Query::SProtocolOption::name) };

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
bool DrawField(SGameField const& field, SGameFormNode& parent, SGameFormNode const* pBaseParent, std::string_view parentPath, SFieldProblem const* pProblem,
	SFormContext const& context)
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
			isEdited = DrawChoice(field, Games::GetFormField(parent, field.name), pBase, field.choices, path, pProblem, {}, {}, {});
			break;
		case EGameFieldKind::Protocol:
			isEdited = DrawChoice(field, Games::GetFormField(parent, field.name), pBase,
				context.definitions | std::views::transform([](Query::SProtocolDefinition const& protocol) { return std::string_view{ protocol.name }; }),
				path, pProblem, DescribeProtocolNamed(context, Games::GetFormField(parent, field.name).text), {}, {});
			break;
		case EGameFieldKind::ProtocolOptions:
			isEdited = DrawProtocolOptions(field, parent, pBaseParent, parentPath, pProblem, context);
			break;
		case EGameFieldKind::TextList:
			isEdited = DrawTextList(field, Games::GetFormField(parent, field.name), pBase, path, pProblem);
			break;
		case EGameFieldKind::Group:
			isEdited = DrawGroup(field, Games::GetFormField(parent, field.name), pBase, path, pProblem, context);
			break;
		case EGameFieldKind::GroupList:
			isEdited = DrawGroupList(field, Games::GetFormField(parent, field.name), pBase, path, pProblem, GetText(parent, ProtocolField), context);
			break;
		case EGameFieldKind::Version:
			break;
	}

	ImGui::PopID();

	return isEdited;
}

//////////////////////////////////////////////////////////////////////////
// Rgb codes carry their colour, so the palette and hex codes are hidden for them; they stay in the form, unwritten.
bool DrawFields(std::string_view schemaParent, SGameFormNode& group, SGameFormNode const* pBaseGroup, std::string_view path, SFieldProblem const* pProblem,
	SFormContext const& context)
{
	SGameFormNode const* const pCodes{ Games::FindFormField(group, CodesField) };
	bool const isRgb{ pCodes != nullptr && pCodes->text == RgbCodes };
	bool isEdited{ false };

	for (SGameField const& field : Games::GetGameFields())
	{
		if (field.parent == schemaParent && !(isRgb && (field.name == PaletteField || field.name == HexCodesField)))
		{
			isEdited = DrawField(field, group, pBaseGroup, path, pProblem, context) || isEdited;
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
bool DrawSection(SGameField const& field, SGameFormNode& form, SGameFormNode const* pDownloaded, SFieldProblem const* pProblem, SFormContext const& context)
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
		isEdited = DrawGroupList(field, node, pBase, path, pProblem, GetText(form, ProtocolField), context);
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
			isEdited = DrawFields(field.name, node, pBase, path, pProblem, context) || isEdited;
		}
	}

	ImGui::PopID();

	return isEdited;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool DrawGameForm(SGameFormNode& form, SGameFormNode const* pDownloaded, SFieldProblem const* pProblem, SFormContext const& context)
{
	bool isEdited{ false };

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (SGameField const& field : Games::GetGameFields())
		{
			if (field.parent.empty() && !IsSection(field))
			{
				isEdited = DrawField(field, form, pDownloaded, {}, pProblem, context) || isEdited;
			}
		}
	}

	for (SGameField const& field : Games::GetGameFields())
	{
		if (IsSection(field))
		{
			isEdited = DrawSection(field, form, pDownloaded, pProblem, context) || isEdited;
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
