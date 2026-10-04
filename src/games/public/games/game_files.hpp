#pragma once

#include "games/editable_game.hpp"
#include "games/game_source.hpp"
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace Lkt::Games
{
// Lower-case letters, digits, '-' and '_', starting with a letter or digit, so a key is also a safe folder name.
bool IsValidKey(std::string_view name);

// Each reads userDir as it is now, so none is out of date after a reload. An error names the file as a problem does.
EGameSource FindGameSource(std::filesystem::path const& userDir, std::string_view key);
// A user game as written, a built-in as built in, a built-in with changes merged with them.
SEditableGame ReadGameText(std::filesystem::path const& userDir, std::string_view key);
// A built-in's text is saved as only what differs from the built-in, and as no file when nothing does.
std::expected<void, std::string> SaveGame(std::filesystem::path const& userDir, std::string_view key, std::string_view text);
// Removes the changes to a built-in; an icon of the user's stays.
std::expected<void, std::string> RevertGame(std::filesystem::path const& userDir, std::string_view key);
// Removes a game of the user's own with everything in its folder.
std::expected<void, std::string> RemoveGame(std::filesystem::path const& userDir, std::string_view key);

std::string_view GetNewGameText();
} // namespace Lkt::Games
