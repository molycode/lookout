#include "downloaded_files.hpp"
#include "sha256.hpp"
#include "games/game_files.hpp"
#include "json/files.hpp"
#include "json/json.hpp"
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
bool HasContent(std::filesystem::path const& path, SIndexFile const& file)
{
	std::expected<std::string, std::error_code> const text{ Json::ReadFile(path, MaxFileSize) };

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
// What game.json says under one string field, or nothing when it is no JSON or lacks it.
std::string ReadGameField(std::filesystem::path const& gameFile, std::string_view field)
{
	std::expected<std::string, std::error_code> const text{ Json::ReadFile(gameFile, MaxFileSize) };
	JsonValue const root = text.has_value() ? JsonValue::parse(*text, nullptr, AllowExceptions, IgnoreComments) : JsonValue{};
	JsonValue::const_iterator const value{ root.is_object() ? root.find(field) : root.cend() };

	return (value != root.cend() && value->is_string()) ? value->get<std::string>() : std::string{};
}

//////////////////////////////////////////////////////////////////////////
void AddNamedProtocols(std::filesystem::path const& gamesFolder, std::set<std::string>& names)
{
	for (std::filesystem::path const& folder : ListEntries(gamesFolder))
	{
		std::string protocol{ ReadGameField(folder / "game.json", "protocol") };

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
		std::string name{ ReadGameField(folder / "game.json", "name") };

		if (Games::IsValidKey(key))
		{
			games.emplace(key, name.empty() ? key : std::move(name));
		}
	}

	return games;
}

//////////////////////////////////////////////////////////////////////////
// Staged beside its final place under a dot name the loader skips, then swapped in; what is left of a failed swap goes.
std::expected<void, std::string> InstallGame(std::filesystem::path const& downloadedDir, std::string const& key,
	std::map<std::string, std::string> const& files)
{
	std::filesystem::path const games{ downloadedDir / "games" };
	std::filesystem::path const staging{ games / std::format(".{}.new", key) };
	std::filesystem::path const previous{ games / std::format(".{}.old", key) };
	std::filesystem::path const target{ games / key };
	std::error_code error{};
	std::expected<void, std::string> result{};

	std::filesystem::remove_all(staging, error);
	std::filesystem::remove_all(previous, error);
	std::filesystem::create_directories(staging, error);

	for (auto const& [name, bytes] : files)
	{
		std::expected<void, std::string> const written{ (error.value() == 0) ? Json::WriteFileAtomically(staging / name, bytes)
			: std::expected<void, std::string>{ std::unexpected{ error.message() } } };

		if (!written.has_value() && result.has_value())
		{
			result = std::unexpected{ std::format("downloaded/games/{}/{}: {}", key, name, written.error()) };
		}
	}

	bool const hadTarget{ std::filesystem::exists(target, error) };

	if (result.has_value() && hadTarget)
	{
		std::filesystem::rename(target, previous, error);
	}

	if (result.has_value() && error.value() == 0)
	{
		std::filesystem::rename(staging, target, error);

		if (error.value() != 0 && hadTarget)
		{
			std::error_code restoreError{};

			std::filesystem::rename(previous, target, restoreError);
		}
	}

	if (result.has_value() && error.value() != 0)
	{
		result = std::unexpected{ std::format("downloaded/games/{}: {}", key, error.message()) };
	}

	std::filesystem::remove_all(staging, error);
	std::filesystem::remove_all(previous, error);

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> InstallProtocol(std::filesystem::path const& downloadedDir, std::string const& name, std::string const& source)
{
	std::filesystem::path const folder{ downloadedDir / "protocols" };
	std::error_code error{};

	std::filesystem::create_directories(folder, error);

	std::expected<void, std::string> const written{ (error.value() == 0)
		? Json::WriteFileAtomically(folder / std::format("{}{}", name, ProtocolExtension), source)
		: std::expected<void, std::string>{ std::unexpected{ error.message() } } };

	return written.has_value() ? written
		: std::expected<void, std::string>{ std::unexpected{ std::format("downloaded/protocols/{}{}: {}", name, ProtocolExtension, written.error()) } };
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
