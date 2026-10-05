#pragma once

#include "games/game_form_node.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Games
{
std::expected<SGameFormNode, std::string> ReadGameForm(std::string_view text);
std::string WriteGameForm(SGameFormNode const& form);
SGameFormNode MakeGameForm();
SGameFormNode MakeGameFormItem(std::string_view listPath);
SGameFormNode const* FindFormField(SGameFormNode const& node, std::string_view name);
SGameFormNode& GetFormField(SGameFormNode& node, std::string_view name);
void ClearFormNotes(SGameFormNode& node);
} // namespace Lkt::Games
