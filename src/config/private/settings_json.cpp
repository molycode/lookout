#include "settings_json.hpp"
#include "json.hpp"
#include "config/default_settings.hpp"
#include "config/first_listed_game.hpp"
#include "config/install_ids.hpp"
#include "config/settings.hpp"
#include "config/window_limits.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/server_address.hpp"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Lkt::Config
{
namespace
{
using Json = nlohmann::ordered_json;

constexpr int IndentWidth{ 1 };
constexpr char IndentCharacter{ '\t' };
constexpr bool EnsureAscii{ false };
constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ true };
constexpr std::string_view FavouritesKey{ "favourites" };
constexpr uint32_t MaxAutoRefreshSeconds{ 3600 };
constexpr std::string_view InstallsKey{ "installs" };

//////////////////////////////////////////////////////////////////////////
constexpr std::string_view ToName(ESortColumn column)
{
	std::string_view name{ "players" };

	switch (column)
	{
		case ESortColumn::Name:
			name = "name";
			break;
		case ESortColumn::Map:
			name = "map";
			break;
		case ESortColumn::Mod:
			name = "mod";
			break;
		case ESortColumn::Mode:
			name = "mode";
			break;
		case ESortColumn::Players:
			name = "players";
			break;
		case ESortColumn::Ping:
			name = "ping";
			break;
		case ESortColumn::Favourite:
			name = "favourite";
			break;
		case ESortColumn::Password:
			name = "password";
			break;
		case ESortColumn::Country:
			name = "country";
			break;
	}

	return name;
}

//////////////////////////////////////////////////////////////////////////
std::string JoinPath(std::string_view parent, std::string_view key)
{
	return parent.empty() ? std::string{ key } : std::format("{}.{}", parent, key);
}

//////////////////////////////////////////////////////////////////////////
void Reject(SSettingsDocument& document, std::string path)
{
	if (document.numInvalid == 0)
	{
		document.firstInvalidPath = std::move(path);
	}

	++document.numInvalid;
}

//////////////////////////////////////////////////////////////////////////
// get<uint32_t>() would also take true, negatives and fractions, so the type is checked as well as the range.
bool IsUnsignedInRange(Json const& json, uint64_t min, uint64_t max)
{
	return json.is_number_unsigned() && json.get<uint64_t>() >= min && json.get<uint64_t>() <= max;
}

//////////////////////////////////////////////////////////////////////////
template<typename TRead>
void ReadValue(Json const& object, std::string_view parent, std::string_view key, SSettingsDocument& document, TRead&& read)
{
	Json::const_iterator const it{ object.find(key) };

	if (it != object.cend() && !read(*it))
	{
		Reject(document, JoinPath(parent, key));
	}
}

//////////////////////////////////////////////////////////////////////////
Json const* FindObject(Json const& object, std::string_view parent, std::string_view key, SSettingsDocument& document)
{
	Json const* pFound{ nullptr };

	ReadValue(object, parent, key, document, [&pFound](Json const& json)
	{
		bool const isObject{ json.is_object() };

		if (isObject)
		{
			pFound = &json;
		}

		return isObject;
	});

	return pFound;
}

//////////////////////////////////////////////////////////////////////////
void ReadBool(Json const& object, std::string_view parent, std::string_view key, bool& value, SSettingsDocument& document)
{
	ReadValue(object, parent, key, document, [&value](Json const& json)
	{
		bool const isValid{ json.is_boolean() };

		if (isValid)
		{
			value = json.get<bool>();
		}

		return isValid;
	});
}

//////////////////////////////////////////////////////////////////////////
void ReadUnsigned(Json const& object, std::string_view parent, std::string_view key, uint32_t min, uint32_t max, uint32_t& value, SSettingsDocument& document)
{
	ReadValue(object, parent, key, document, [min, max, &value](Json const& json)
	{
		bool const isValid{ IsUnsignedInRange(json, min, max) };

		if (isValid)
		{
			value = static_cast<uint32_t>(json.get<uint64_t>());
		}

		return isValid;
	});
}

//////////////////////////////////////////////////////////////////////////
// A NUL would silently cut the string short wherever it later becomes a C string, such as a command line.
bool IsCleanString(Json const& json)
{
	return json.is_string() && !json.get_ref<std::string const&>().contains('\0');
}

//////////////////////////////////////////////////////////////////////////
void ReadString(Json const& object, std::string_view parent, std::string_view key, std::string& value, SSettingsDocument& document)
{
	ReadValue(object, parent, key, document, [&value](Json const& json)
	{
		bool const isValid{ IsCleanString(json) };

		if (isValid)
		{
			value = json.get<std::string>();
		}

		return isValid;
	});
}

//////////////////////////////////////////////////////////////////////////
void ReadWindow(Json const& object, std::string_view path, SWindowSettings& window, SSettingsDocument& document)
{
	ReadUnsigned(object, path, "width", MinWindowWidth, MaxWindowSide, window.width, document);
	ReadUnsigned(object, path, "height", MinWindowHeight, MaxWindowSide, window.height, document);
	ReadBool(object, path, "maximized", window.isMaximized, document);
	ReadUnsigned(object, path, "detailsWidth", 1, MaxWindowSide, window.detailsWidth, document);
	ReadString(object, path, "layout", window.layout, document);
}

//////////////////////////////////////////////////////////////////////////
void ReadFilter(Json const& object, std::string_view path, SServerFilter& filter, SSettingsDocument& document)
{
	ReadString(object, path, "search", filter.search, document);
	ReadBool(object, path, "showEmpty", filter.showEmpty, document);
	ReadBool(object, path, "showFull", filter.showFull, document);
	ReadValue(object, path, "maxPing", document, [&filter](Json const& json)
	{
		bool const isNoLimit{ json.is_null() };
		bool const isLimit{ IsUnsignedInRange(json, 0, NoPingLimit) };

		if (isNoLimit)
		{
			filter.maxPingMs = NoPingLimit;
		}
		else if (isLimit)
		{
			filter.maxPingMs = static_cast<uint32_t>(json.get<uint64_t>());
		}

		return isNoLimit || isLimit;
	});
	ReadString(object, path, "mod", filter.mod, document);
	ReadString(object, path, "country", filter.country, document);
}

//////////////////////////////////////////////////////////////////////////
void ReadSort(Json const& object, std::string_view path, SSortOrder& sort, SSettingsDocument& document)
{
	ReadValue(object, path, "column", document, [&sort](Json const& json)
	{
		bool isKnown{ false };

		if (json.is_string())
		{
			for (size_t index{ 0 }; !isKnown && index < NumSortColumns; ++index)
			{
				ESortColumn const column{ static_cast<ESortColumn>(index) };

				isKnown = json.get_ref<std::string const&>() == ToName(column);

				if (isKnown)
				{
					sort.column = column;
				}
			}
		}

		return isKnown;
	});
	ReadBool(object, path, "ascending", sort.isAscending, document);
}

//////////////////////////////////////////////////////////////////////////
void ReadFavourites(Json const& object, std::string_view path, std::vector<Query::SServerAddress>& favourites, SSettingsDocument& document)
{
	ReadValue(object, path, FavouritesKey, document, [path, &favourites, &document](Json const& json)
	{
		bool const isArray{ json.is_array() };

		if (isArray)
		{
			size_t index{ 0 };

			for (Json const& entry : json)
			{
				bool isValid{ entry.is_string() };

				if (isValid)
				{
					std::expected<Query::SServerAddress, Query::EParseError> const address{ Query::ParseAddress(entry.get_ref<std::string const&>()) };

					isValid = address.has_value() && !std::ranges::contains(favourites, *address);

					if (isValid)
					{
						favourites.emplace_back(*address);
					}
				}

				if (!isValid)
				{
					Reject(document, std::format("{}.{}[{}]", path, FavouritesKey, index));
				}

				++index;
			}
		}

		return isArray;
	});
}

//////////////////////////////////////////////////////////////////////////
std::optional<SGameInstall> ReadInstall(Json const& json, std::span<SGameInstall const> earlier)
{
	std::optional<SGameInstall> install{};

	if (json.is_object())
	{
		Json::const_iterator const id{ json.find("id") };
		Json::const_iterator const name{ json.find("name") };
		Json::const_iterator const folder{ json.find("folder") };
		Json::const_iterator const command{ json.find("command") };
		bool const hasOneLocation{ (folder != json.cend()) != (command != json.cend()) };
		Json::const_iterator const location{ (folder != json.cend()) ? folder : command };
		bool const isFolder{ folder != json.cend() };
		bool const isValid{ id != json.cend() && IsUnsignedInRange(*id, 1, MaxInstallId)
			&& FindInstall(earlier, static_cast<uint32_t>(id->get<uint64_t>())) == nullptr
			&& (name == json.cend() || IsCleanString(*name))
			&& hasOneLocation && IsCleanString(*location)
			&& (!isFolder || std::filesystem::path{ location->get_ref<std::string const&>() }.is_absolute()) };

		if (isValid)
		{
			install = SGameInstall{ static_cast<uint32_t>(id->get<uint64_t>()), (name != json.cend()) ? name->get<std::string>() : std::string{},
				isFolder ? EInstallKind::Folder : EInstallKind::Command, location->get<std::string>() };
		}
	}

	return install;
}

//////////////////////////////////////////////////////////////////////////
void ReadInstalls(Json const& object, std::string_view path, std::vector<SGameInstall>& installs, SSettingsDocument& document)
{
	ReadValue(object, path, InstallsKey, document, [path, &installs, &document](Json const& json)
	{
		bool const isArray{ json.is_array() };

		if (isArray)
		{
			size_t index{ 0 };

			for (Json const& entry : json)
			{
				std::optional<SGameInstall> install{ ReadInstall(entry, installs) };

				if (install.has_value())
				{
					installs.emplace_back(std::move(*install));
				}
				else
				{
					Reject(document, std::format("{}.{}[{}]", path, InstallsKey, index));
				}

				++index;
			}
		}

		return isArray;
	});
}

//////////////////////////////////////////////////////////////////////////
// Format 1 kept one custom command per game: it becomes an install.
void MigrateCustomCommand(Json const& object, std::string_view path, SGameSettings& game, SSettingsDocument& document)
{
	std::string command{};

	ReadString(object, path, "customCommand", command, document);

	if (!command.empty())
	{
		game.installs.emplace_back(SGameInstall{ NextInstallId(game.installs), {}, EInstallKind::Command, std::move(command) });
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadGame(Json const& object, std::string_view path, SGameSettings& game, SSettingsDocument& document)
{
	ReadBool(object, path, "listed", game.isListed, document);

	Json const* const pFilter{ FindObject(object, path, "filter", document) };

	if (pFilter != nullptr)
	{
		ReadFilter(*pFilter, JoinPath(path, "filter"), game.filter, document);
	}

	Json const* const pSort{ FindObject(object, path, "sort", document) };

	if (pSort != nullptr)
	{
		ReadSort(*pSort, JoinPath(path, "sort"), game.sort, document);
	}

	ReadInstalls(object, path, game.installs, document);
	ReadFavourites(object, path, game.favourites, document);
	MigrateCustomCommand(object, path, game, document);
}

//////////////////////////////////////////////////////////////////////////
// The sidebar must offer at least one game, and the selected one among them.
void CheckListedGames(SSettingsDocument& document)
{
	std::optional<Query::EGame> const firstListed{ FindFirstListedGame(document.settings) };

	if (!firstListed.has_value())
	{
		Reject(document, "games");

		for (SGameSettings& game : document.settings.games)
		{
			game.isListed = SGameSettings{}.isListed;
		}
	}
	else if (!document.settings.games[static_cast<size_t>(document.settings.selectedGame)].isListed)
	{
		Reject(document, "game");
		document.settings.selectedGame = *firstListed;
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadDocument(Json const& root, SSettingsDocument& document)
{
	ReadValue(root, {}, "version", document, [&document](Json const& json)
	{
		bool const isValid{ IsUnsignedInRange(json, 0, std::numeric_limits<uint32_t>::max()) };

		if (isValid)
		{
			document.version = static_cast<uint32_t>(json.get<uint64_t>());
		}

		return isValid;
	});

	Json const* const pWindow{ FindObject(root, {}, "window", document) };

	if (pWindow != nullptr)
	{
		ReadWindow(*pWindow, "window", document.settings.window, document);
	}

	ReadValue(root, {}, "game", document, [&document](Json const& json)
	{
		Query::SGameDefinition const* const pGame{ json.is_string() ? Query::FindGame(json.get_ref<std::string const&>()) : nullptr };

		if (pGame != nullptr)
		{
			document.settings.selectedGame = pGame->game;
		}

		return pGame != nullptr;
	});

	ReadUnsigned(root, {}, "autoRefreshSeconds", 0, MaxAutoRefreshSeconds, document.settings.autoRefreshSeconds, document);

	Json const* const pGames{ FindObject(root, {}, "games", document) };

	if (pGames != nullptr)
	{
		for (Query::SGameDefinition const& game : Query::GetGameCatalog())
		{
			std::string const path{ JoinPath("games", game.key) };
			Json const* const pGame{ FindObject(*pGames, "games", game.key, document) };

			if (pGame != nullptr)
			{
				ReadGame(*pGame, path, document.settings.games[static_cast<size_t>(game.game)], document);
			}
		}
	}

	CheckListedGames(document);
}

//////////////////////////////////////////////////////////////////////////
Json WriteWindow(SWindowSettings const& window)
{
	Json object = Json::object();

	object["width"] = window.width;
	object["height"] = window.height;
	object["maximized"] = window.isMaximized;
	object["detailsWidth"] = window.detailsWidth;
	object["layout"] = window.layout;

	return object;
}

//////////////////////////////////////////////////////////////////////////
Json WriteFilter(SServerFilter const& filter)
{
	Json object = Json::object();
	Json maxPing{};

	if (filter.maxPingMs != NoPingLimit)
	{
		maxPing = filter.maxPingMs;
	}

	object["search"] = filter.search;
	object["showEmpty"] = filter.showEmpty;
	object["showFull"] = filter.showFull;
	object["maxPing"] = std::move(maxPing);
	object["mod"] = filter.mod;
	object["country"] = filter.country;

	return object;
}

//////////////////////////////////////////////////////////////////////////
Json WriteSort(SSortOrder const& sort)
{
	Json object = Json::object();

	object["column"] = ToName(sort.column);
	object["ascending"] = sort.isAscending;

	return object;
}

//////////////////////////////////////////////////////////////////////////
Json WriteGame(SGameSettings const& game)
{
	Json object = Json::object();
	Json installs = Json::array();
	Json favourites = Json::array();

	for (SGameInstall const& install : game.installs)
	{
		Json entry = Json::object();

		entry["id"] = install.id;
		entry["name"] = install.name;
		entry[(install.kind == EInstallKind::Folder) ? "folder" : "command"] = install.location;
		installs.emplace_back(std::move(entry));
	}

	for (Query::SServerAddress const& address : game.favourites)
	{
		favourites.emplace_back(Query::FormatAddress(address));
	}

	object["listed"] = game.isListed;
	object["filter"] = WriteFilter(game.filter);
	object["sort"] = WriteSort(game.sort);
	object[InstallsKey] = std::move(installs);
	object["favourites"] = std::move(favourites);

	return object;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::string WriteSettingsJson(SSettings const& settings)
{
	Json root = Json::object();
	Json games = Json::object();

	for (Query::SGameDefinition const& game : Query::GetGameCatalog())
	{
		games[std::string{ game.key }] = WriteGame(settings.games[static_cast<size_t>(game.game)]);
	}

	root["version"] = SettingsVersion;
	root["window"] = WriteWindow(settings.window);
	root["game"] = Query::GetGame(settings.selectedGame).key;
	root["games"] = std::move(games);
	root["autoRefreshSeconds"] = settings.autoRefreshSeconds;

	// Replacing invalid UTF-8 rather than failing, which without exceptions would be an abort.
	return root.dump(IndentWidth, IndentCharacter, EnsureAscii, Json::error_handler_t::replace) + '\n';
}

//////////////////////////////////////////////////////////////////////////
std::expected<SSettingsDocument, ESettingsJsonError> ReadSettingsJson(std::string_view text)
{
	std::expected<SSettingsDocument, ESettingsJsonError> result{ std::unexpected{ ESettingsJsonError::NotJson } };

	Json const root = Json::parse(text, nullptr, AllowExceptions, IgnoreComments);

	if (root.is_object())
	{
		SSettingsDocument document{};

		document.settings = MakeDefaultSettings();
		document.version = SettingsVersion;
		ReadDocument(root, document);
		result = std::move(document);
	}
	else if (!root.is_discarded())
	{
		result = std::unexpected{ ESettingsJsonError::NotAnObject };
	}

	return result;
}
} // namespace Lkt::Config
