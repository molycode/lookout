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
// Where the downloads go in the data folder, beside the user's own games and protocols, which change them.
std::filesystem::path GetDownloadedDir(std::filesystem::path const& userDir);

// Each reads userDir as it is now, so none is out of date after a reload. An error names the file as a problem does.
EGameSource FindGameSource(std::filesystem::path const& userDir, std::string_view key);
// A user game as written, a downloaded one as downloaded, or merged with the user's changes to it.
SEditableGame ReadGameText(std::filesystem::path const& userDir, std::string_view key);
// A downloaded game's text is saved as only what differs from the download, and as no file when nothing does.
std::expected<void, std::string> SaveGame(std::filesystem::path const& userDir, std::string_view key, std::string_view text);
// Removes the changes to a downloaded game; an icon of the user's stays.
std::expected<void, std::string> RevertGame(std::filesystem::path const& userDir, std::string_view key);
// Removes a game of the user's own with everything in its folder.
std::expected<void, std::string> RemoveGame(std::filesystem::path const& userDir, std::string_view key);

std::string_view GetNewGameText();
} // namespace Lkt::Games
