#include "games/game_files.hpp"
#include "embedded_games.hpp"
#include "embedded_text.hpp"
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

//////////////////////////////////////////////////////////////////////////
std::optional<std::string_view> FindBuiltinText(std::string_view key)
{
	auto const it{ std::ranges::find(Embedded::Games, key, [](Embedded::SEmbeddedFile const& file) { return file.name.substr(0, file.name.find('/')); }) };

	return (it != Embedded::Games.end()) ? std::optional<std::string_view>{ Embedded::AsText(it->bytes) } : std::nullopt;
}

//////////////////////////////////////////////////////////////////////////
std::filesystem::path GetGameFolder(std::filesystem::path const& userDir, std::string_view key)
{
	return userDir / "games" / key;
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
EGameSource FindGameSource(std::filesystem::path const& userDir, std::string_view key)
{
	bool const isBuiltin{ FindBuiltinText(key).has_value() };
	bool const hasUserDir{ !userDir.empty() && IsValidKey(key) };
	std::error_code error{};
	EGameSource source{ EGameSource::None };

	if (isBuiltin && hasUserDir && std::filesystem::exists(GetGameFolder(userDir, key) / GameFileName, error))
	{
		source = EGameSource::Patched;
	}
	else if (isBuiltin)
	{
		source = EGameSource::Builtin;
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
	std::optional<std::string_view> const builtin{ FindBuiltinText(key) };
	std::expected<std::string, std::error_code> const userText{ (!userDir.empty() && IsValidKey(key))
		? Json::ReadFile(GetGameFolder(userDir, key) / GameFileName, MaxUserFileSize)
		: std::expected<std::string, std::error_code>{ std::unexpected{ std::make_error_code(std::errc::no_such_file_or_directory) } } };
	SEditableGame game{};

	if (builtin.has_value() && userText.has_value())
	{
		std::expected<std::string, std::string> merged{ ApplyPatch(*builtin, *userText) };

		if (merged.has_value())
		{
			game.text = std::move(*merged);
		}
		else
		{
			game.text = *builtin;
			game.problem = std::format("games/{}/{}: {}", key, GameFileName, merged.error());
		}
	}
	else if (builtin.has_value())
	{
		game.text = *builtin;

		if (userText.error() != std::errc::no_such_file_or_directory)
		{
			game.problem = std::format("games/{}/{}: {}", key, GameFileName, userText.error().message());
		}
	}
	else if (userText.has_value())
	{
		game.text = *userText;
	}
	else
	{
		game.problem = std::format("games/{}/{}: {}", key, GameFileName, userText.error().message());
	}

	return game;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> SaveGame(std::filesystem::path const& userDir, std::string_view key, std::string_view text)
{
	TGE_ASSERT(!userDir.empty() && IsValidKey(key), "A game is saved under a key that is not a safe folder name");

	std::filesystem::path const folder{ GetGameFolder(userDir, key) };
	std::optional<std::string_view> const builtin{ FindBuiltinText(key) };
	std::expected<std::optional<std::string>, std::string> const file{ builtin.has_value() ? MakePatch(*builtin, text)
		: std::expected<std::optional<std::string>, std::string>{ std::string{ text } } };
	std::expected<void, std::string> result{};

	if (!file.has_value())
	{
		result = std::unexpected{ std::format("games/{}/{}: {}", key, GameFileName, file.error()) };
	}
	else if (file->has_value() && builtin.has_value())
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
	TGE_ASSERT(!userDir.empty() && FindBuiltinText(key).has_value(), "Only a built-in game can be reverted");

	return RemoveGameFile(GetGameFolder(userDir, key), key);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> RemoveGame(std::filesystem::path const& userDir, std::string_view key)
{
	TGE_ASSERT(!userDir.empty() && IsValidKey(key) && !FindBuiltinText(key).has_value(), "Only a game of the user's own can be removed");

	std::error_code error{};
	std::expected<void, std::string> result{};

	std::filesystem::remove_all(GetGameFolder(userDir, key), error);

	if (error.value() != 0)
	{
		result = std::unexpected{ std::format("games/{}: {}", key, error.message()) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::string_view GetNewGameText()
{
	return Embedded::AsText(Embedded::NewGame);
}
} // namespace Lkt::Games
