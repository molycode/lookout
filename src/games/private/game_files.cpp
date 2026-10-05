#include "games/game_files.hpp"
#include "merge_patch.hpp"
#include "user_file_size.hpp"
#include "json/files.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <format>
#include <optional>
#include <system_error>
#include <utility>

namespace Lkt::Games
{
namespace
{
constexpr std::string_view GameFileName{ "game.json" };
constexpr std::string_view DownloadedFolderName{ "downloaded" };

//////////////////////////////////////////////////////////////////////////
std::filesystem::path GetGameFolder(std::filesystem::path const& userDir, std::string_view key)
{
	return userDir / "games" / key;
}

//////////////////////////////////////////////////////////////////////////
bool IsDownloaded(std::filesystem::path const& userDir, std::string_view key)
{
	std::error_code error{};

	return !userDir.empty() && IsValidKey(key) && std::filesystem::exists(GetGameFolder(GetDownloadedDir(userDir), key) / GameFileName, error);
}

//////////////////////////////////////////////////////////////////////////
// Absent when the game was not downloaded, or its file cannot be read.
std::optional<std::string> ReadDownloadedText(std::filesystem::path const& userDir, std::string_view key)
{
	std::expected<std::string, std::error_code> text{ IsDownloaded(userDir, key)
		? Json::ReadFile(GetGameFolder(GetDownloadedDir(userDir), key) / GameFileName, MaxUserFileSize)
		: std::expected<std::string, std::error_code>{ std::unexpected{ std::make_error_code(std::errc::no_such_file_or_directory) } } };

	return text.has_value() ? std::optional<std::string>{ std::move(*text) } : std::nullopt;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> WriteGameFile(std::filesystem::path const& folder, std::string_view key, std::string_view text)
{
	std::error_code error{};
	std::expected<void, std::string> result{};

	std::filesystem::create_directories(folder, error);

	if (error.value() == 0)
	{
		std::expected<void, std::string> const written{ Json::WriteFileAtomically(folder / GameFileName, text) };

		if (!written.has_value())
		{
			result = std::unexpected{ std::format("games/{}/{}: {}", key, GameFileName, written.error()) };
		}
	}
	else
	{
		result = std::unexpected{ std::format("games/{}: {}", key, error.message()) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// The folder goes too once nothing else, such as an icon, is left in it.
std::expected<void, std::string> RemoveGameFile(std::filesystem::path const& folder, std::string_view key)
{
	std::error_code fileError{};
	std::error_code folderError{};
	std::expected<void, std::string> result{};

	std::filesystem::remove(folder / GameFileName, fileError);

	if (fileError.value() == 0)
	{
		std::filesystem::remove(folder, folderError);
	}

	if (fileError.value() != 0)
	{
		result = std::unexpected{ std::format("games/{}/{}: {}", key, GameFileName, fileError.message()) };
	}
	else if (folderError.value() != 0 && folderError != std::errc::directory_not_empty && folderError != std::errc::no_such_file_or_directory)
	{
		result = std::unexpected{ std::format("games/{}: {}", key, folderError.message()) };
	}

	return result;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool IsValidKey(std::string_view name)
{
	auto const isStart{ [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); } };

	return !name.empty() && isStart(name.front()) && std::ranges::all_of(name, [&isStart](char c) { return isStart(c) || c == '-' || c == '_'; });
}

//////////////////////////////////////////////////////////////////////////
std::filesystem::path GetDownloadedDir(std::filesystem::path const& userDir)
{
	return userDir.empty() ? std::filesystem::path{} : userDir / DownloadedFolderName;
}

//////////////////////////////////////////////////////////////////////////
EGameSource FindGameSource(std::filesystem::path const& userDir, std::string_view key)
{
	bool const isDownloaded{ IsDownloaded(userDir, key) };
	bool const hasUserDir{ !userDir.empty() && IsValidKey(key) };
	std::error_code error{};
	EGameSource source{ EGameSource::None };

	if (isDownloaded && std::filesystem::exists(GetGameFolder(userDir, key) / GameFileName, error))
	{
		source = EGameSource::Patched;
	}
	else if (isDownloaded)
	{
		source = EGameSource::Downloaded;
	}
	else if (hasUserDir && std::filesystem::is_directory(GetGameFolder(userDir, key), error))
	{
		source = EGameSource::User;
	}

	return source;
}

//////////////////////////////////////////////////////////////////////////
SEditableGame ReadGameText(std::filesystem::path const& userDir, std::string_view key)
{
	std::optional<std::string> const downloaded{ ReadDownloadedText(userDir, key) };
	std::expected<std::string, std::error_code> const userText{ (!userDir.empty() && IsValidKey(key))
		? Json::ReadFile(GetGameFolder(userDir, key) / GameFileName, MaxUserFileSize)
		: std::expected<std::string, std::error_code>{ std::unexpected{ std::make_error_code(std::errc::no_such_file_or_directory) } } };
	SEditableGame game{};

	if (downloaded.has_value())
	{
		game.downloaded = *downloaded;
	}

	if (downloaded.has_value() && userText.has_value())
	{
		std::expected<std::string, std::string> merged{ ApplyPatch(*downloaded, *userText) };

		if (merged.has_value())
		{
			game.text = std::move(*merged);
		}
		else
		{
			game.text = *downloaded;
			game.problem = std::format("games/{}/{}: {}", key, GameFileName, merged.error());
		}
	}
	else if (downloaded.has_value())
	{
		game.text = *downloaded;

		if (userText.error() != std::errc::no_such_file_or_directory)
		{
			game.problem = std::format("games/{}/{}: {}", key, GameFileName, userText.error().message());
		}
	}
	else if (userText.has_value())
	{
		game.text = *userText;
	}
	else if (userText.error() != std::errc::no_such_file_or_directory)
	{
		game.problem = std::format("games/{}/{}: {}", key, GameFileName, userText.error().message());
	}

	return game;
}

//////////////////////////////////////////////////////////////////////////
// Against what the editor opened, so a download updated meanwhile keeps its new values; one removed meanwhile leaves
// nothing to patch, so the game is saved whole.
std::expected<void, std::string> SaveGame(std::filesystem::path const& userDir, std::string_view key, std::string_view text, std::string_view downloaded)
{
	TGE_ASSERT(!userDir.empty() && IsValidKey(key), "A game is saved under a key that is not a safe folder name");

	std::filesystem::path const folder{ GetGameFolder(userDir, key) };
	bool const isPatch{ !downloaded.empty() && IsDownloaded(userDir, key) };
	std::expected<std::optional<std::string>, std::string> const file{ isPatch ? MakePatch(downloaded, text)
		: std::expected<std::optional<std::string>, std::string>{ std::string{ text } } };
	std::expected<void, std::string> result{};

	if (!file.has_value())
	{
		result = std::unexpected{ std::format("games/{}/{}: {}", key, GameFileName, file.error()) };
	}
	else if (file->has_value() && isPatch)
	{
		result = WriteGameFile(folder, key, std::format("{}\n", **file));
	}
	else if (file->has_value())
	{
		result = WriteGameFile(folder, key, **file);
	}
	else
	{
		result = RemoveGameFile(folder, key);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> RevertGame(std::filesystem::path const& userDir, std::string_view key)
{
	TGE_ASSERT(IsDownloaded(userDir, key), "Only a downloaded game can be reverted");

	return RemoveGameFile(GetGameFolder(userDir, key), key);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> RemoveGame(std::filesystem::path const& userDir, std::string_view key)
{
	TGE_ASSERT(!userDir.empty() && IsValidKey(key) && !IsDownloaded(userDir, key), "Only a game of the user's own can be removed");

	std::error_code error{};
	std::expected<void, std::string> result{};

	std::filesystem::remove_all(GetGameFolder(userDir, key), error);

	if (error.value() != 0)
	{
		result = std::unexpected{ std::format("games/{}: {}", key, error.message()) };
	}

	return result;
}
} // namespace Lkt::Games
