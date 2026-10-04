#pragma once

#include "download/offer_state.hpp"
#include "index_game.hpp"
#include "index_protocol.hpp"
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Lkt::Download
{
// How the downloaded copy of a game compares with the index: its files and its protocol's, by checksum.
EOfferState FindInstalledState(std::filesystem::path const& downloadedDir, SIndexGame const& game, SIndexProtocol const& protocol);
bool IsProtocolCurrent(std::filesystem::path const& downloadedDir, SIndexProtocol const& protocol);

// Keys of the games in the folder, and the name each one's game.json gives, or its key.
std::map<std::string, std::string> ListDownloadedGames(std::filesystem::path const& downloadedDir);

// The game's folder is replaced whole, through renames, so a game is never half old and half new.
std::expected<void, std::string> InstallGame(std::filesystem::path const& downloadedDir, std::string const& key,
	std::map<std::string, std::string> const& files);
std::expected<void, std::string> InstallProtocol(std::filesystem::path const& downloadedDir, std::string const& name, std::string const& source);
std::expected<void, std::string> RemoveGame(std::filesystem::path const& downloadedDir, std::string const& key);
// Removes each downloaded protocol that neither a downloaded game nor one of the user's games names any more.
std::vector<std::string> RemoveUnusedProtocols(std::filesystem::path const& downloadedDir, std::filesystem::path const& userDir);
} // namespace Lkt::Download
