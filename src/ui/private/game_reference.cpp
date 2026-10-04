#include "game_reference.hpp"
#include "format_to.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "games/game_fields.hpp"
#include "query/game_catalog.hpp"
#include "query/protocol_definition.hpp"
#include <imgui.h>
#include <array>
#include <string_view>

namespace Lkt::Ui
{
namespace
{
//////////////////////////////////////////////////////////////////////////
// The description below its name: the reference is too narrow for the two side by side.
void DrawEntry(std::string_view name, std::string_view description)
{
	ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().amber);
	ImGui::TextUnformatted(name.data(), name.data() + name.size());
	ImGui::PopStyleColor();
	ImGui::Indent();
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(description.data(), description.data() + description.size());
	ImGui::PopTextWrapPos();
	ImGui::Unindent();
	ImGui::Spacing();
}

//////////////////////////////////////////////////////////////////////////
void DrawFields()
{
	std::array<char, 96> buffer{};

	for (Games::SGameField const& field : Games::GetGameFields())
	{
		DrawEntry(field.parent.empty() ? field.name : FormatTo(buffer, "{}.{}", field.parent, field.name), field.description);
	}
}

//////////////////////////////////////////////////////////////////////////
void DrawProtocols()
{
	std::array<char, 96> nameBuffer{};
	std::array<char, 512> descriptionBuffer{};

	for (Query::SProtocolDefinition const& protocol : Query::GetProtocolCatalog())
	{
		if (protocol.options.empty())
		{
			DrawEntry(protocol.name, "No options.");
		}

		for (Query::SProtocolOption const& option : protocol.options)
		{
			DrawEntry(FormatTo(nameBuffer, "{}: {}", protocol.name, option.name),
				option.isRequired ? FormatTo(descriptionBuffer, "Required. {}", option.description) : std::string_view{ option.description });
		}
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void DrawGameReference()
{
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted("A field \"//name\" beside the field \"name\" is a comment on it.");
	ImGui::PopTextWrapPos();
	ImGui::SeparatorText("Fields");
	DrawFields();
	ImGui::SeparatorText("Protocols and their options");
	DrawProtocols();
}
} // namespace Lkt::Ui
