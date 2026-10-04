#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace Lkt::Games
{
// RFC 7386: what the patch names replaces the built-in's, a null removes it, the rest stays. Tab-indented.
std::expected<std::string, std::string> ApplyPatch(std::string_view builtinText, std::string_view patchText);
// The patch that turns the built-in into the game; none when the two hold the same JSON.
std::expected<std::optional<std::string>, std::string> MakePatch(std::string_view builtinText, std::string_view gameText);
} // namespace Lkt::Games
