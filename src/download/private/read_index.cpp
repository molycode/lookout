#include "read_index.hpp"
#include "games/game_files.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <utility>

namespace Lkt::Download
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ false };
constexpr uint64_t IndexFormat{ 1 };
constexpr size_t CommitLength{ 40 };
constexpr size_t Sha256Length{ 64 };
constexpr size_t MaxScriptSize{ 256 * 1024 };

// A file a game may hold, and the most it may weigh.
constexpr std::array<std::pair<std::string_view, size_t>, 3> GameFiles{ {
	{ "game.json", 64 * 1024 },
	{ "icon.png", 256 * 1024 },
	{ "icon-licence.txt", 64 * 1024 }
} };

//////////////////////////////////////////////////////////////////////////
bool IsLowerHex(std::string_view text, size_t length)
{
	return text.size() == length && std::ranges::all_of(text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

//////////////////////////////////////////////////////////////////////////
// Only the first problem is kept, so the message names where the index first went wrong.
void Fail(std::string& problem, std::string_view path, std::string_view reason)
{
	if (problem.empty())
	{
		problem = std::format("index.json: {}: {}", path, reason);
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadFile(JsonValue const& json, std::string_view path, std::string_view name, size_t maxSize, SIndexFile& file, std::string& problem)
{
	JsonValue::const_iterator const size{ json.is_object() ? json.find("size") : json.cend() };
	JsonValue::const_iterator const sha256{ json.is_object() ? json.find("sha256") : json.cend() };

	file.name = name;

	if (!json.is_object())
	{
		Fail(problem, path, "must be an object");
	}
	else if (size == json.cend() || !size->is_number_unsigned() || size->get<uint64_t>() > maxSize)
	{
		Fail(problem, std::format("{}.size", path), std::format("must be a whole number of bytes up to {}", maxSize));
	}
	else if (sha256 == json.cend() || !sha256->is_string() || !IsLowerHex(sha256->get_ref<std::string const&>(), Sha256Length))
	{
		Fail(problem, std::format("{}.sha256", path), "must be a SHA-256 in lower-case hex");
	}
	else
	{
		file.size = static_cast<size_t>(size->get<uint64_t>());
		file.sha256 = sha256->get<std::string>();
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadGame(std::string const& key, JsonValue const& json, SIndexGame& game, std::string& problem)
{
	std::string const path{ std::format("games.{}", key) };
	JsonValue::const_iterator const name{ json.is_object() ? json.find("name") : json.cend() };
	JsonValue::const_iterator const format{ json.is_object() ? json.find("format") : json.cend() };
	JsonValue::const_iterator const protocol{ json.is_object() ? json.find("protocol") : json.cend() };
	JsonValue::const_iterator const files{ json.is_object() ? json.find("files") : json.cend() };

	game.key = key;

	if (!Games::IsValidKey(key))
	{
		Fail(problem, path, "is not a key: small letters, digits, '-' and '_'");
	}
	else if (name == json.cend() || !name->is_string() || name->get_ref<std::string const&>().empty())
	{
		Fail(problem, std::format("{}.name", path), "must be a non-empty string");
	}
	else if (format == json.cend() || !format->is_number_unsigned())
	{
		Fail(problem, std::format("{}.format", path), "must be a whole number");
	}
	else if (protocol == json.cend() || !protocol->is_string() || !Games::IsValidKey(protocol->get_ref<std::string const&>()))
	{
		Fail(problem, std::format("{}.protocol", path), "must name a protocol");
	}
	else if (files == json.cend() || !files->is_object() || !files->contains("game.json"))
	{
		Fail(problem, std::format("{}.files", path), "must be an object holding game.json");
	}
	else
	{
		game.name = name->get<std::string>();
		game.format = format->get<uint64_t>();
		game.protocol = protocol->get<std::string>();

		for (auto const& item : files->items())
		{
			std::string const filePath{ std::format("{}.files.{}", path, item.key()) };
			auto const known{ std::ranges::find(GameFiles, std::string_view{ item.key() }, &std::pair<std::string_view, size_t>::first) };

			if (known != GameFiles.end())
			{
				ReadFile(item.value(), filePath, item.key(), known->second, game.files.emplace_back(), problem);
			}
			else
			{
				Fail(problem, filePath, "is not a file a game holds");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadProtocol(std::string const& name, JsonValue const& json, SIndexProtocol& protocol, std::string& problem)
{
	std::string const path{ std::format("protocols.{}", name) };
	JsonValue::const_iterator const api{ json.is_object() ? json.find("api") : json.cend() };
	JsonValue::const_iterator const version{ json.is_object() ? json.find("version") : json.cend() };

	protocol.name = name;

	if (!Games::IsValidKey(name))
	{
		Fail(problem, path, "is not a protocol name: small letters, digits, '-' and '_'");
	}
	else if (api == json.cend() || !api->is_number_integer())
	{
		Fail(problem, std::format("{}.api", path), "must be a whole number");
	}
	else if (version != json.cend() && (!version->is_number_unsigned() || version->get<uint64_t>() == 0))
	{
		Fail(problem, std::format("{}.version", path), "must be a whole number from 1");
	}
	else
	{
		protocol.api = api->get<int64_t>();
		protocol.version = (version != json.cend()) ? std::optional<uint64_t>{ version->get<uint64_t>() } : std::nullopt;
		ReadFile(json, path, std::format("{}.lua", name), MaxScriptSize, protocol.file, problem);
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadContent(JsonValue const& root, SGameIndex& index, std::string& problem)
{
	JsonValue::const_iterator const commit{ root.find("commit") };
	JsonValue::const_iterator const games{ root.find("games") };
	JsonValue::const_iterator const protocols{ root.find("protocols") };

	if (commit == root.cend() || !commit->is_string() || !IsLowerHex(commit->get_ref<std::string const&>(), CommitLength))
	{
		Fail(problem, "commit", "must be a commit's 40 lower-case hex digits");
	}
	else if (games == root.cend() || !games->is_object())
	{
		Fail(problem, "games", "must be an object");
	}
	else if (protocols == root.cend() || !protocols->is_object())
	{
		Fail(problem, "protocols", "must be an object");
	}
	else
	{
		index.commit = commit->get<std::string>();

		for (auto const& item : protocols->items())
		{
			ReadProtocol(item.key(), item.value(), index.protocols.emplace_back(), problem);
		}

		for (auto const& item : games->items())
		{
			ReadGame(item.key(), item.value(), index.games.emplace_back(), problem);
		}

		for (SIndexGame const& game : index.games)
		{
			if (problem.empty() && !std::ranges::contains(index.protocols, game.protocol, &SIndexProtocol::name))
			{
				Fail(problem, std::format("games.{}.protocol", game.key), std::format("'{}' is not one of its protocols", game.protocol));
			}
		}
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<SGameIndex, std::string> ReadIndex(std::string_view text)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	JsonValue::const_iterator const format{ root.is_object() ? root.find("index") : root.cend() };
	SGameIndex index{};
	std::string problem{};

	if (root.is_discarded())
	{
		problem = std::format("index.json: it is not valid JSON: {}", Json::DescribeSyntaxError(text, IgnoreComments));
	}
	else if (format == root.cend() || !format->is_number_unsigned())
	{
		Fail(problem, "index", "must be the index's format, a whole number");
	}
	else if (format->get<uint64_t>() > IndexFormat)
	{
		Fail(problem, "index", std::format("is {}, which needs a newer Lookout", format->get<uint64_t>()));
	}
	else
	{
		ReadContent(root, index, problem);
	}

	std::expected<SGameIndex, std::string> result{ std::move(index) };

	if (!problem.empty())
	{
		result = std::unexpected{ std::move(problem) };
	}

	return result;
}
} // namespace Lkt::Download
