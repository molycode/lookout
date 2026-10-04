#include "games/load_games.hpp"
#include "embedded_games.hpp"
#include "game_json.hpp"
#include "json/files.hpp"
#include "json/json.hpp"
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
using JsonValue = nlohmann::ordered_json;
using Scripts = std::vector<std::unique_ptr<Script::CProtocolScript>>;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ true };
constexpr size_t MaxFileSize{ 1024 * 1024 };
constexpr std::string_view ProtocolExtension{ ".lua" };
constexpr std::string_view KeyRule{ "its name must use only lower-case letters, digits, '-' and '_'" };

//////////////////////////////////////////////////////////////////////////
std::string_view AsText(std::span<unsigned char const> bytes)
{
	return std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() };
}

//////////////////////////////////////////////////////////////////////////
// Starting with a letter or digit, so a key is also a safe folder name.
bool IsKey(std::string_view name)
{
	auto const isStart{ [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); } };

	return !name.empty() && isStart(name.front()) && std::ranges::all_of(name, [&isStart](char c) { return isStart(c) || c == '-' || c == '_'; });
}

//////////////////////////////////////////////////////////////////////////
// By name, without dot entries; a missing folder holds nothing.
std::vector<std::filesystem::directory_entry> ListFolder(std::filesystem::path const& folder, std::string_view shownAs, std::vector<std::string>& problems)
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
		problems.emplace_back(std::format("{}: {}", shownAs, error.message()));
	}

	std::ranges::sort(entries, {}, [](std::filesystem::directory_entry const& entry) { return entry.path().filename(); });

	return entries;
}

//////////////////////////////////////////////////////////////////////////
// An absent file is no error, only nothing.
std::expected<std::optional<std::string>, std::string> ReadUserFile(std::filesystem::path const& path, std::string_view shownAs)
{
	std::expected<std::string, std::error_code> text{ Json::ReadFile(path, MaxFileSize) };
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
std::map<std::string, std::string> ReadUserProtocols(std::filesystem::path const& userDir, std::vector<std::string>& problems)
{
	std::map<std::string, std::string> sources{};

	for (std::filesystem::directory_entry const& entry : ListFolder(userDir / "protocols", "protocols", problems))
	{
		std::filesystem::path const& path{ entry.path() };
		std::string const shownAs{ std::format("protocols/{}", path.filename().string()) };
		std::string const name{ path.stem().string() };

		if (path.extension() == ProtocolExtension)
		{
			std::expected<std::optional<std::string>, std::string> text{ IsKey(name) ? ReadUserFile(path, shownAs)
				: std::unexpected{ std::format("{}: {}", shownAs, KeyRule) } };

			if (!text.has_value())
			{
				problems.emplace_back(std::move(text.error()));
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
// A user script replaces the built-in of its name; when it cannot be loaded, the built-in stays.
void LoadProtocols(std::map<std::string, std::string> const& userSources, SGameContent& content, Scripts& scripts)
{
	std::map<std::string, std::string_view> builtins{};
	std::set<std::string> names{};

	for (Embedded::SEmbeddedFile const& file : Embedded::Protocols)
	{
		std::string const name{ file.name.substr(0, file.name.rfind('.')) };

		builtins.emplace(name, AsText(file.bytes));
		names.insert(name);
	}

	for (auto const& [name, source] : userSources)
	{
		names.insert(name);
	}

	for (std::string const& name : names)
	{
		auto pScript{ std::make_unique<Script::CProtocolScript>() };
		auto const user{ userSources.find(name) };
		auto const builtin{ builtins.find(name) };
		std::string_view source{};
		bool isLoaded{ false };

		if (user != userSources.end())
		{
			std::expected<void, std::string> const loaded{ pScript->Initialize(name, user->second) };

			isLoaded = loaded.has_value();
			source = user->second;

			if (!isLoaded)
			{
				content.problems.emplace_back(std::format("protocols/{}{}: {}", name, ProtocolExtension, loaded.error()));
				pScript->Terminate();
			}
		}

		if (!isLoaded && builtin != builtins.end())
		{
			std::expected<void, std::string> const loaded{ pScript->Initialize(name, builtin->second) };

			isLoaded = loaded.has_value();
			source = builtin->second;

			if (!isLoaded)
			{
				content.problems.emplace_back(std::format("The built-in protocol '{}' cannot be loaded: {}", name, loaded.error()));
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
// Each kind of conversation is started once with the game's options, so a game the script cannot talk for fails here.
std::expected<void, std::string> TryConversations(Script::CProtocolScript& script, std::map<std::string, std::string> const& options)
{
	std::expected<void, std::string> result{};

	for (Script::EConversationKind const kind : { Script::EConversationKind::Master, Script::EConversationKind::Server })
	{
		Script::SConversation conversation{ kind, 0 };
		std::expected<Script::SScriptAction, std::string> const started{ script.Start(conversation, options) };

		script.End(conversation);

		if (!started.has_value() && result.has_value())
		{
			result = std::unexpected{ started.error() };
		}
	}

	return result;
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
// A merge patch: what it names replaces the built-in's, a null removes it, the rest stays.
std::expected<std::string, std::string> ApplyPatch(std::string_view builtinText, std::string_view patchText)
{
	JsonValue game = JsonValue::parse(builtinText, nullptr, AllowExceptions, IgnoreComments);
	JsonValue const patch = JsonValue::parse(patchText, nullptr, AllowExceptions, IgnoreComments);
	std::expected<std::string, std::string> result{ std::unexpected{ std::string{ "it is not valid JSON" } } };

	if (patch.is_object())
	{
		game.merge_patch(patch);
		result = game.dump(-1, ' ', false, JsonValue::error_handler_t::replace);
	}
	else if (!patch.is_discarded())
	{
		result = std::unexpected{ std::string{ "a change to a built-in game must be a JSON object" } };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// A user game.json is the whole game, or over a built-in a patch to it; when it cannot be used, the built-in stays.
void AddGame(std::string_view key, std::optional<std::string_view> builtin, std::span<unsigned char const> builtinIcon,
	std::optional<std::filesystem::path> const& userFolder, SGameContent& content, Scripts const& scripts)
{
	using UserFile = std::expected<std::optional<std::string>, std::string>;

	std::string const shownAs{ std::format("games/{}/game.json", key) };
	UserFile const userText{ userFolder.has_value() ? ReadUserFile(*userFolder / "game.json", shownAs) : UserFile{} };
	UserFile const userIcon{ userFolder.has_value() ? ReadUserFile(*userFolder / "icon.png", std::format("games/{}/icon.png", key)) : UserFile{} };
	bool const hasUserText{ userText.has_value() && userText->has_value() };
	bool const hasUserIcon{ userIcon.has_value() && userIcon->has_value() };
	std::span<std::byte const> const icon{ hasUserIcon ? std::as_bytes(std::span{ **userIcon }) : std::as_bytes(builtinIcon) };
	std::optional<Query::SGameDefinition> game{};

	for (UserFile const* pFile : { &userText, &userIcon })
	{
		if (!pFile->has_value())
		{
			content.problems.emplace_back(pFile->error());
		}
	}

	if (hasUserText)
	{
		std::expected<std::string, std::string> const text{ builtin.has_value() ? ApplyPatch(*builtin, **userText)
			: std::expected<std::string, std::string>{ **userText } };
		std::expected<Query::SGameDefinition, std::string> read{ text.has_value() ? ReadGame(key, *text, icon, content, scripts)
			: std::expected<Query::SGameDefinition, std::string>{ std::unexpected{ text.error() } } };

		if (read.has_value())
		{
			game = std::move(*read);
		}
		else
		{
			content.problems.emplace_back(std::format("{}: {}", shownAs, read.error()));
		}
	}

	if (!game.has_value() && builtin.has_value())
	{
		std::expected<Query::SGameDefinition, std::string> read{ ReadGame(key, *builtin, icon, content, scripts) };

		if (read.has_value())
		{
			game = std::move(*read);
		}
		else
		{
			content.problems.emplace_back(std::format("The built-in game '{}' cannot be read: {}", key, read.error()));
		}
	}
	else if (!game.has_value() && userText.has_value() && !hasUserText)
	{
		content.problems.emplace_back(std::format("games/{}: has no game.json", key));
	}

	if (game.has_value())
	{
		content.games.emplace_back(std::move(*game));
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SGameContent LoadGames(std::filesystem::path const& userDir)
{
	SGameContent content{};
	Scripts scripts{};
	bool const hasUserDir{ !userDir.empty() };

	LoadProtocols(hasUserDir ? ReadUserProtocols(userDir, content.problems) : std::map<std::string, std::string>{}, content, scripts);

	std::map<std::string, std::string_view> builtins{};
	std::map<std::string, std::span<unsigned char const>> builtinIcons{};
	std::map<std::string, std::filesystem::path> userFolders{};
	std::set<std::string> keys{};

	for (Embedded::SEmbeddedFile const& file : Embedded::Games)
	{
		std::string const key{ file.name.substr(0, file.name.find('/')) };

		builtins.emplace(key, AsText(file.bytes));
		keys.insert(key);
	}

	for (Embedded::SEmbeddedFile const& file : Embedded::GameIcons)
	{
		builtinIcons.emplace(file.name.substr(0, file.name.find('/')), file.bytes);
	}

	std::vector<std::filesystem::directory_entry> const userEntries{ hasUserDir ? ListFolder(userDir / "games", "games", content.problems)
		: std::vector<std::filesystem::directory_entry>{} };

	for (std::filesystem::directory_entry const& entry : userEntries)
	{
		std::error_code error{};
		std::string const name{ entry.path().filename().string() };

		if (entry.is_directory(error) && !IsKey(name))
		{
			content.problems.emplace_back(std::format("games/{}: {}", name, KeyRule));
		}
		else if (entry.is_directory(error))
		{
			userFolders.emplace(name, entry.path());
			keys.insert(name);
		}
	}

	for (std::string const& key : keys)
	{
		auto const builtin{ builtins.find(key) };
		auto const builtinIcon{ builtinIcons.find(key) };
		auto const userFolder{ userFolders.find(key) };

		AddGame(key, (builtin != builtins.end()) ? std::optional<std::string_view>{ builtin->second } : std::nullopt,
			(builtinIcon != builtinIcons.end()) ? builtinIcon->second : std::span<unsigned char const>{},
			(userFolder != userFolders.end()) ? std::optional<std::filesystem::path>{ userFolder->second } : std::nullopt, content, scripts);
	}

	std::ranges::sort(content.games, {}, &Query::SGameDefinition::name);

	for (std::unique_ptr<Script::CProtocolScript> const& pScript : scripts)
	{
		pScript->Terminate();
	}

	return content;
}
} // namespace Lkt::Games
