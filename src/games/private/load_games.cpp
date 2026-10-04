#include "games/load_games.hpp"
#include "game_json.hpp"
#include "layer_name.hpp"
#include "merge_patch.hpp"
#include "try_conversations.hpp"
#include "user_file_size.hpp"
#include "games/game_files.hpp"
#include "json/files.hpp"
#include "script/protocol_script.hpp"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Lkt::Games
{
namespace
{
using Scripts = std::vector<std::unique_ptr<Script::CProtocolScript>>;

constexpr std::string_view ProtocolExtension{ ".lua" };
constexpr std::string_view KeyRule{ "its name must use only lower-case letters, digits, '-' and '_'" };

//////////////////////////////////////////////////////////////////////////
// By name, without dot entries; a missing folder holds nothing.
std::vector<std::filesystem::directory_entry> ListFolder(std::filesystem::path const& folder, std::string_view shownAs,
	std::vector<Query::SGameProblem>& problems)
{
	std::vector<std::filesystem::directory_entry> entries{};
	std::error_code error{};

	for (std::filesystem::directory_iterator it{ folder, error }, end{}; error.value() == 0 && it != end; it.increment(error))
	{
		if (!it->path().filename().string().starts_with('.'))
		{
			entries.emplace_back(*it);
		}
	}

	if (error.value() != 0 && error != std::errc::no_such_file_or_directory)
	{
		problems.emplace_back(Query::SGameProblem{ std::format("{}: {}", shownAs, error.message()), {} });
	}

	std::ranges::sort(entries, {}, [](std::filesystem::directory_entry const& entry) { return entry.path().filename(); });

	return entries;
}

//////////////////////////////////////////////////////////////////////////
// An absent file is no error, only nothing.
std::expected<std::optional<std::string>, std::string> ReadUserFile(std::filesystem::path const& path, std::string_view shownAs)
{
	std::expected<std::string, std::error_code> text{ Json::ReadFile(path, MaxUserFileSize) };
	std::expected<std::optional<std::string>, std::string> result{};

	if (text.has_value())
	{
		result = std::optional<std::string>{ std::move(*text) };
	}
	else if (text.error() != std::errc::no_such_file_or_directory)
	{
		result = std::unexpected{ std::format("{}: {}", shownAs, text.error().message()) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Where a problem says it is: the downloaded layer is named by its folder, the user's is the data folder itself.
std::string Shown(std::string_view layer, std::string_view path)
{
	return layer.empty() ? std::string{ path } : std::format("{}/{}", layer, path);
}

//////////////////////////////////////////////////////////////////////////
// Its protocols/<name>.lua by name; nothing from an empty folder path.
std::map<std::string, std::string> ReadProtocolSources(std::filesystem::path const& folder, std::string_view layer,
	std::vector<Query::SGameProblem>& problems)
{
	std::map<std::string, std::string> sources{};
	std::vector<std::filesystem::directory_entry> const entries{ folder.empty() ? std::vector<std::filesystem::directory_entry>{}
		: ListFolder(folder / "protocols", Shown(layer, "protocols"), problems) };

	for (std::filesystem::directory_entry const& entry : entries)
	{
		std::filesystem::path const& path{ entry.path() };
		std::string const shownAs{ Shown(layer, std::format("protocols/{}", path.filename().string())) };
		std::string const name{ path.stem().string() };

		if (path.extension() == ProtocolExtension)
		{
			std::expected<std::optional<std::string>, std::string> text{ IsValidKey(name) ? ReadUserFile(path, shownAs)
				: std::unexpected{ std::format("{}: {}", shownAs, KeyRule) } };

			if (!text.has_value())
			{
				problems.emplace_back(Query::SGameProblem{ std::move(text.error()), {} });
			}
			else if (text->has_value())
			{
				sources.emplace(name, std::move(**text));
			}
		}
	}

	return sources;
}

//////////////////////////////////////////////////////////////////////////
// Its games/<key> folders by key; nothing from an empty folder path.
std::map<std::string, std::filesystem::path> ListGameFolders(std::filesystem::path const& folder, std::string_view layer,
	std::vector<Query::SGameProblem>& problems)
{
	std::map<std::string, std::filesystem::path> games{};
	std::vector<std::filesystem::directory_entry> const entries{ folder.empty() ? std::vector<std::filesystem::directory_entry>{}
		: ListFolder(folder / "games", Shown(layer, "games"), problems) };

	for (std::filesystem::directory_entry const& entry : entries)
	{
		std::error_code error{};
		std::string const name{ entry.path().filename().string() };

		if (entry.is_directory(error) && !IsValidKey(name))
		{
			problems.emplace_back(Query::SGameProblem{ std::format("{}: {}", Shown(layer, std::format("games/{}", name)), KeyRule), {} });
		}
		else if (entry.is_directory(error))
		{
			games.emplace(name, entry.path());
		}
	}

	return games;
}

//////////////////////////////////////////////////////////////////////////
// A user script replaces the downloaded one of its name; when it cannot be loaded, the downloaded one stays.
void LoadProtocols(std::map<std::string, std::string> const& downloaded, std::string_view layer, std::map<std::string, std::string> const& user,
	SGameContent& content, Scripts& scripts)
{
	std::set<std::string> names{};

	for (auto const& [name, source] : downloaded)
	{
		names.insert(name);
	}

	for (auto const& [name, source] : user)
	{
		names.insert(name);
	}

	for (std::string const& name : names)
	{
		auto pScript{ std::make_unique<Script::CProtocolScript>() };
		auto const userSource{ user.find(name) };
		auto const downloadedSource{ downloaded.find(name) };
		std::string_view source{};
		bool isLoaded{ false };

		if (userSource != user.end())
		{
			std::expected<void, std::string> const loaded{ pScript->Initialize(name, userSource->second) };

			isLoaded = loaded.has_value();
			source = userSource->second;

			if (!isLoaded)
			{
				content.problems.emplace_back(Query::SGameProblem{ std::format("protocols/{}{}: {}", name, ProtocolExtension, loaded.error()), {} });
				pScript->Terminate();
			}
		}

		if (!isLoaded && downloadedSource != downloaded.end())
		{
			std::expected<void, std::string> const loaded{ pScript->Initialize(name, downloadedSource->second) };

			isLoaded = loaded.has_value();
			source = downloadedSource->second;

			if (!isLoaded)
			{
				content.problems.emplace_back(Query::SGameProblem{
					std::format("{}: {}", Shown(layer, std::format("protocols/{}{}", name, ProtocolExtension)), loaded.error()), {} });
				pScript->Terminate();
			}
		}

		if (isLoaded)
		{
			std::span<Query::SProtocolOption const> const options{ pScript->GetOptions() };

			content.protocols.emplace_back(Query::SProtocolDefinition{ name, std::string{ source },
				std::vector<Query::SProtocolOption>{ options.begin(), options.end() } });
			scripts.emplace_back(std::move(pScript));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
std::expected<Query::SGameDefinition, std::string> ReadGame(std::string_view key, std::string_view text, std::span<std::byte const> icon,
	SGameContent const& content, Scripts const& scripts)
{
	std::expected<Query::SGameDefinition, std::string> game{ ReadGameJson(text, content.protocols) };

	if (game.has_value())
	{
		std::expected<void, std::string> const tried{ TryConversations(*scripts[static_cast<size_t>(game->protocol)], game->protocolOptions) };

		if (tried.has_value())
		{
			game->key = key;
			game->icon.assign(icon.begin(), icon.end());
		}
		else
		{
			game = std::unexpected{ tried.error() };
		}
	}

	return game;
}

//////////////////////////////////////////////////////////////////////////
// A user game.json is the whole game, or over a downloaded one a patch to it; when it cannot be used, the downloaded one
// stays. Only the user's files can be fixed in the editor, so only their problems name the game.
void AddGame(std::string_view key, std::optional<std::filesystem::path> const& downloadedFolder, std::string_view layer,
	std::optional<std::filesystem::path> const& userFolder, SGameContent& content, Scripts const& scripts)
{
	using File = std::expected<std::optional<std::string>, std::string>;

	std::string const shownAs{ std::format("games/{}/game.json", key) };
	std::string const downloadedShownAs{ Shown(layer, shownAs) };
	File const downloadedText{ downloadedFolder.has_value() ? ReadUserFile(*downloadedFolder / "game.json", downloadedShownAs) : File{} };
	File const downloadedIcon{ downloadedFolder.has_value() ? ReadUserFile(*downloadedFolder / "icon.png", Shown(layer, std::format("games/{}/icon.png", key)))
		: File{} };
	File const userText{ userFolder.has_value() ? ReadUserFile(*userFolder / "game.json", shownAs) : File{} };
	File const userIcon{ userFolder.has_value() ? ReadUserFile(*userFolder / "icon.png", std::format("games/{}/icon.png", key)) : File{} };
	bool const hasDownloadedText{ downloadedText.has_value() && downloadedText->has_value() };
	bool const hasUserText{ userText.has_value() && userText->has_value() };
	bool const hasUserIcon{ userIcon.has_value() && userIcon->has_value() };
	bool const hasDownloadedIcon{ downloadedIcon.has_value() && downloadedIcon->has_value() };
	std::span<std::byte const> const icon{ hasUserIcon ? std::as_bytes(std::span{ **userIcon })
		: (hasDownloadedIcon ? std::as_bytes(std::span{ **downloadedIcon }) : std::span<std::byte const>{}) };
	std::optional<Query::SGameDefinition> game{};

	for (File const* pFile : { &downloadedText, &downloadedIcon, &userIcon })
	{
		if (!pFile->has_value())
		{
			content.problems.emplace_back(Query::SGameProblem{ pFile->error(), {} });
		}
	}

	if (!userText.has_value())
	{
		content.problems.emplace_back(Query::SGameProblem{ userText.error(), std::string{ key } });
	}

	if (hasUserText)
	{
		std::expected<std::string, std::string> const text{ hasDownloadedText ? ApplyPatch(**downloadedText, **userText)
			: std::expected<std::string, std::string>{ **userText } };
		std::expected<Query::SGameDefinition, std::string> read{ text.has_value() ? ReadGame(key, *text, icon, content, scripts)
			: std::expected<Query::SGameDefinition, std::string>{ std::unexpected{ text.error() } } };

		if (read.has_value())
		{
			game = std::move(*read);
		}
		else
		{
			content.problems.emplace_back(Query::SGameProblem{ std::format("{}: {}", shownAs, read.error()), std::string{ key } });
		}
	}

	if (!game.has_value() && hasDownloadedText)
	{
		std::expected<Query::SGameDefinition, std::string> read{ ReadGame(key, **downloadedText, icon, content, scripts) };

		if (read.has_value())
		{
			game = std::move(*read);
		}
		else
		{
			content.problems.emplace_back(Query::SGameProblem{ std::format("{}: {}", downloadedShownAs, read.error()), {} });
		}
	}
	else if (!game.has_value() && userText.has_value() && !hasUserText && !downloadedFolder.has_value())
	{
		content.problems.emplace_back(Query::SGameProblem{ std::format("games/{}: has no game.json", key), std::string{ key } });
	}
	else if (!game.has_value() && downloadedFolder.has_value() && downloadedText.has_value() && !hasDownloadedText)
	{
		content.problems.emplace_back(Query::SGameProblem{ std::format("{}: has no game.json", Shown(layer, std::format("games/{}", key))), {} });
	}

	if (game.has_value())
	{
		content.games.emplace_back(std::move(*game));
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SGameContent LoadGames(std::filesystem::path const& downloadedDir, std::filesystem::path const& userDir)
{
	SGameContent content{};
	Scripts scripts{};
	std::string const layer{ NameLayer(downloadedDir) };
	std::map<std::string, std::string> const downloadedProtocols{ ReadProtocolSources(downloadedDir, layer, content.problems) };
	std::map<std::string, std::string> const userProtocols{ ReadProtocolSources(userDir, {}, content.problems) };

	LoadProtocols(downloadedProtocols, layer, userProtocols, content, scripts);

	std::map<std::string, std::filesystem::path> const downloadedGames{ ListGameFolders(downloadedDir, layer, content.problems) };
	std::map<std::string, std::filesystem::path> const userGames{ ListGameFolders(userDir, {}, content.problems) };
	std::set<std::string> keys{};

	for (auto const& [key, folder] : downloadedGames)
	{
		keys.insert(key);
	}

	for (auto const& [key, folder] : userGames)
	{
		keys.insert(key);
	}

	for (std::string const& key : keys)
	{
		auto const downloadedFolder{ downloadedGames.find(key) };
		auto const userFolder{ userGames.find(key) };

		AddGame(key, (downloadedFolder != downloadedGames.end()) ? std::optional<std::filesystem::path>{ downloadedFolder->second } : std::nullopt, layer,
			(userFolder != userGames.end()) ? std::optional<std::filesystem::path>{ userFolder->second } : std::nullopt, content, scripts);
	}

	std::ranges::sort(content.games, {}, &Query::SGameDefinition::name);

	for (std::unique_ptr<Script::CProtocolScript> const& pScript : scripts)
	{
		pScript->Terminate();
	}

	return content;
}
} // namespace Lkt::Games
