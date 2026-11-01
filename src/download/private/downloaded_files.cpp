#include "downloaded_files.hpp"
#include "sha256.hpp"
#include "games/game_files.hpp"
#include "json/json.hpp"
#include <tge/io/read_whole_file.hpp>
#include <tge/io/write_file_atomically.hpp>
#include <algorithm>
#include <format>
#include <set>
#include <system_error>

namespace Lkt::Download
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ false };
constexpr size_t MaxFileSize{ 1024 * 1024 };
constexpr std::string_view ProtocolExtension{ ".lua" };

//////////////////////////////////////////////////////////////////////////
std::filesystem::path GetStagingFolder(std::filesystem::path const& downloadedDir, std::string const& key)
{
	return downloadedDir / "games" / std::format(".{}.new", key);
}

//////////////////////////////////////////////////////////////////////////
std::filesystem::path GetPreviousFolder(std::filesystem::path const& downloadedDir, std::string const& key)
{
	return downloadedDir / "games" / std::format(".{}.old", key);
}

//////////////////////////////////////////////////////////////////////////
bool HasContent(std::filesystem::path const& path, SIndexFile const& file)
{
	std::expected<std::string, std::error_code> const text{ Tge::IO::ReadWholeFile(path, MaxFileSize) };

	return text.has_value() && text->size() == file.size && HashSha256(*text) == file.sha256;
}

//////////////////////////////////////////////////////////////////////////
// By name, without dot entries, which are this code's own staging folders; a missing folder holds nothing.
std::vector<std::filesystem::path> ListEntries(std::filesystem::path const& folder)
{
	std::vector<std::filesystem::path> entries{};
	std::error_code error{};

	for (std::filesystem::directory_iterator it{ folder, error }, end{}; error.value() == 0 && it != end; it.increment(error))
	{
		if (!it->path().filename().string().starts_with('.'))
		{
			entries.emplace_back(it->path());
		}
	}

	std::ranges::sort(entries);

	return entries;
}

//////////////////////////////////////////////////////////////////////////
JsonValue ReadGame(std::filesystem::path const& gameFile)
{
	std::expected<std::string, std::error_code> const text{ Tge::IO::ReadWholeFile(gameFile, MaxFileSize) };

	return text.has_value() ? JsonValue::parse(*text, nullptr, AllowExceptions, IgnoreComments) : JsonValue{};
}

//////////////////////////////////////////////////////////////////////////
// What object says under one string field, or nothing when it is no object or lacks it.
std::string ReadString(JsonValue const& object, std::string_view field)
{
	JsonValue::const_iterator const value{ object.is_object() ? object.find(field) : object.cend() };

	return (value != object.cend() && value->is_string()) ? value->get<std::string>() : std::string{};
}

//////////////////////////////////////////////////////////////////////////
void AddNamedProtocols(std::filesystem::path const& gamesFolder, std::set<std::string>& names)
{
	for (std::filesystem::path const& folder : ListEntries(gamesFolder))
	{
		std::string protocol{ ReadString(ReadGame(folder / "game.json"), "protocol") };

		if (!protocol.empty())
		{
			names.insert(std::move(protocol));
		}
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
EOfferState FindInstalledState(std::filesystem::path const& downloadedDir, SIndexGame const& game, SIndexProtocol const& protocol)
{
	std::filesystem::path const folder{ downloadedDir / "games" / game.key };
	std::error_code error{};
	EOfferState state{ EOfferState::NotInstalled };

	if (std::filesystem::exists(folder / "game.json", error))
	{
		bool const hasIndexFiles{ std::ranges::all_of(game.files, [&folder](SIndexFile const& file) { return HasContent(folder / file.name, file); }) };
		bool const hasOtherFiles{ std::ranges::any_of(ListEntries(folder), [&game](std::filesystem::path const& path)
		{
			return !std::ranges::contains(game.files, path.filename().string(), &SIndexFile::name);
		}) };

		state = (hasIndexFiles && !hasOtherFiles && IsProtocolCurrent(downloadedDir, protocol)) ? EOfferState::Installed : EOfferState::UpdateAvailable;
	}

	return state;
}

//////////////////////////////////////////////////////////////////////////
bool IsProtocolCurrent(std::filesystem::path const& downloadedDir, SIndexProtocol const& protocol)
{
	return HasContent(downloadedDir / "protocols" / protocol.file.name, protocol.file);
}

//////////////////////////////////////////////////////////////////////////
std::map<std::string, std::string> ListDownloadedGames(std::filesystem::path const& downloadedDir)
{
	std::map<std::string, std::string> games{};

	for (std::filesystem::path const& folder : ListEntries(downloadedDir / "games"))
	{
		std::string const key{ folder.filename().string() };
		std::string name{ ReadString(ReadGame(folder / "game.json"), "name") };

		if (Games::IsValidKey(key))
		{
			games.emplace(key, name.empty() ? key : std::move(name));
		}
	}

	return games;
}

//////////////////////////////////////////////////////////////////////////
// Staged beside its final place under a dot name the loader skips; a stale staging folder that cannot go would bring
// its own files along, so that fails the game too. Nothing staged is left behind a failure.
std::expected<void, std::string> StageGame(std::filesystem::path const& downloadedDir, std::string const& key,
	std::map<std::string, std::string> const& files)
{
	std::filesystem::path const staging{ GetStagingFolder(downloadedDir, key) };
	std::error_code error{};
	std::expected<void, std::string> result{};

	std::filesystem::remove_all(staging, error);

	if (error.value() == 0)
	{
		std::filesystem::create_directories(staging, error);
	}

	for (auto const& [name, bytes] : files)
	{
		std::expected<void, std::error_code> const written{ (error.value() == 0) ? Tge::IO::WriteFileAtomically(staging / name, bytes)
			: std::expected<void, std::error_code>{ std::unexpected{ error } } };

		if (!written.has_value() && result.has_value())
		{
			result = std::unexpected{ std::format("downloaded/games/{}/{}: {}", key, name, written.error().message()) };
		}
	}

	if (!result.has_value())
	{
		DiscardStagedGame(downloadedDir, key);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// One attempt, safe to repeat after any failure: a folder an earlier attempt set aside goes first, as a rename never
// replaces a folder on Windows, and the game in place comes back when the new one cannot take its place.
std::expected<void, std::error_code> SwapInGame(std::filesystem::path const& downloadedDir, std::string const& key)
{
	std::filesystem::path const staging{ GetStagingFolder(downloadedDir, key) };
	std::filesystem::path const previous{ GetPreviousFolder(downloadedDir, key) };
	std::filesystem::path const target{ downloadedDir / "games" / key };
	std::error_code error{};
	std::error_code ignored{};

	std::filesystem::remove_all(previous, error);

	bool const hadTarget{ error.value() == 0 && std::filesystem::exists(target, error) };

	if (hadTarget)
	{
		std::filesystem::rename(target, previous, error);
	}

	if (error.value() == 0)
	{
		std::filesystem::rename(staging, target, error);

		if (error.value() != 0 && hadTarget)
		{
			std::filesystem::rename(previous, target, ignored);
		}
	}

	if (error.value() == 0)
	{
		std::filesystem::remove_all(previous, ignored);
	}

	return (error.value() == 0) ? std::expected<void, std::error_code>{} : std::unexpected{ error };
}

//////////////////////////////////////////////////////////////////////////
// What cannot go now goes at the game's next install; the loader skips it meanwhile.
void DiscardStagedGame(std::filesystem::path const& downloadedDir, std::string const& key)
{
	std::error_code ignored{};

	std::filesystem::remove_all(GetStagingFolder(downloadedDir, key), ignored);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> InstallProtocol(std::filesystem::path const& downloadedDir, std::string const& name, std::string const& source)
{
	std::filesystem::path const folder{ downloadedDir / "protocols" };
	std::error_code error{};

	std::filesystem::create_directories(folder, error);

	std::expected<void, std::error_code> const written{ (error.value() == 0)
		? Tge::IO::WriteFileAtomically(folder / std::format("{}{}", name, ProtocolExtension), source)
		: std::expected<void, std::error_code>{ std::unexpected{ error } } };

	return written.has_value() ? std::expected<void, std::string>{}
		: std::expected<void, std::string>{ std::unexpected{ std::format("downloaded/protocols/{}{}: {}", name, ProtocolExtension, written.error().message()) } };
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> RemoveGame(std::filesystem::path const& downloadedDir, std::string const& key)
{
	std::error_code error{};
	std::expected<void, std::string> result{};

	if (Games::IsValidKey(key))
	{
		std::filesystem::remove_all(downloadedDir / "games" / key, error);
	}

	if (error.value() != 0)
	{
		result = std::unexpected{ std::format("downloaded/games/{}: {}", key, error.message()) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::string> RemoveUnusedProtocols(std::filesystem::path const& downloadedDir, std::filesystem::path const& userDir)
{
	std::set<std::string> named{};
	std::vector<std::string> problems{};

	AddNamedProtocols(downloadedDir / "games", named);

	if (!userDir.empty())
	{
		AddNamedProtocols(userDir / "games", named);
	}

	for (std::filesystem::path const& path : ListEntries(downloadedDir / "protocols"))
	{
		std::error_code error{};

		if (path.extension() == ProtocolExtension && !named.contains(path.stem().string()))
		{
			std::filesystem::remove(path, error);
		}

		if (error.value() != 0)
		{
			problems.emplace_back(std::format("downloaded/protocols/{}: {}", path.filename().string(), error.message()));
		}
	}

	return problems;
}
} // namespace Lkt::Download
