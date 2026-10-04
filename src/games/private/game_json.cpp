#include "game_json.hpp"
#include "json/json.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <span>
#include <utility>
#include <vector>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr bool AllowExceptions{ false };
constexpr bool IgnoreComments{ true };
constexpr uint64_t Format{ 1 };
constexpr uint64_t MaxPort{ 65535 };

constexpr std::array<std::string_view, 10> GameFields{ "format", "name", "protocol", "textStyle", "protocolOptions", "masters", "keys",
	"modes", "foreignServers", "launch" };
constexpr std::array<std::string_view, 2> MasterFields{ "host", "port" };
constexpr std::array<std::string_view, 5> KeyFields{ "hostname", "map", "maxPlayers", "password", "mods" };
constexpr std::array<std::string_view, 3> ModeFields{ "key", "value", "label" };
constexpr std::array<std::string_view, 2> MatchFields{ "key", "value" };
constexpr std::array<std::string_view, 4> LaunchFields{ "desktopFiles", "installDir", "program", "requiredFiles" };

constexpr std::array<std::pair<std::string_view, Query::ETextStyle>, 3> TextStyles{ {
	{ "ascii7", Query::ETextStyle::Ascii7 },
	{ "quake3", Query::ETextStyle::Quake3 },
	{ "enemyTerritory", Query::ETextStyle::EnemyTerritory }
} };

//////////////////////////////////////////////////////////////////////////
std::string JoinPath(std::string_view parent, std::string_view key)
{
	return parent.empty() ? std::string{ key } : std::format("{}.{}", parent, key);
}

//////////////////////////////////////////////////////////////////////////
// Only the first problem is kept, so the message names where the file first went wrong.
void Fail(std::string& problem, std::string_view path, std::string_view reason)
{
	if (problem.empty())
	{
		problem = std::format("{}: {}", path, reason);
	}
}

//////////////////////////////////////////////////////////////////////////
// A field format 1 does not know is a typo or a later format's, so it is never skipped silently.
void CheckFields(JsonValue const& object, std::string_view path, std::span<std::string_view const> fields, std::string& problem)
{
	for (auto const& item : object.items())
	{
		if (!std::ranges::contains(fields, std::string_view{ item.key() }))
		{
			Fail(problem, JoinPath(path, item.key()), "is not a field of format 1");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Every string names a rule, a host or a path, so an empty one or a NUL is always a mistake.
void ReadString(JsonValue const& json, std::string_view path, std::string& value, std::string& problem)
{
	bool const isValid{ json.is_string() && !json.get_ref<std::string const&>().empty() && !json.get_ref<std::string const&>().contains('\0') };

	if (isValid)
	{
		value = json.get<std::string>();
	}
	else
	{
		Fail(problem, path, "must be a non-empty string without NUL");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadStrings(JsonValue const& json, std::string_view path, std::vector<std::string>& values, std::string& problem)
{
	if (json.is_array())
	{
		size_t index{ 0 };

		for (JsonValue const& element : json)
		{
			ReadString(element, std::format("{}[{}]", path, index), values.emplace_back(), problem);
			++index;
		}
	}
	else
	{
		Fail(problem, path, "must be an array of strings");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadRequiredString(JsonValue const& object, std::string_view parent, std::string_view key, std::string& value, std::string& problem)
{
	JsonValue::const_iterator const it{ object.find(key) };

	if (it != object.cend())
	{
		ReadString(*it, JoinPath(parent, key), value, problem);
	}
	else
	{
		Fail(problem, JoinPath(parent, key), "is missing");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadRequiredStrings(JsonValue const& object, std::string_view parent, std::string_view key, std::vector<std::string>& values, std::string& problem)
{
	JsonValue::const_iterator const it{ object.find(key) };

	if (it != object.cend())
	{
		ReadStrings(*it, JoinPath(parent, key), values, problem);
	}
	else
	{
		Fail(problem, JoinPath(parent, key), "is missing");
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename TEnum, size_t NumNames>
void ReadName(JsonValue const& object, std::string_view key, std::array<std::pair<std::string_view, TEnum>, NumNames> const& names, TEnum& value,
	std::string& problem)
{
	std::string name{};

	ReadRequiredString(object, {}, key, name, problem);

	auto const it{ std::ranges::find(names, std::string_view{ name }, &std::pair<std::string_view, TEnum>::first) };

	if (it != names.end())
	{
		value = it->second;
	}
	else if (!name.empty())
	{
		std::string known{};

		for (std::pair<std::string_view, TEnum> const& entry : names)
		{
			known += std::format("{}{}", known.empty() ? "" : ", ", entry.first);
		}

		Fail(problem, key, std::format("'{}' is not one of {}", name, known));
	}
}

//////////////////////////////////////////////////////////////////////////
JsonValue const* FindObject(JsonValue const& object, std::string_view key, bool isRequired, std::string& problem)
{
	JsonValue const* pFound{ nullptr };
	JsonValue::const_iterator const it{ object.find(key) };

	if (it != object.cend() && it->is_object())
	{
		pFound = &*it;
	}
	else if (it != object.cend())
	{
		Fail(problem, key, "must be an object");
	}
	else if (isRequired)
	{
		Fail(problem, key, "is missing");
	}

	return pFound;
}

//////////////////////////////////////////////////////////////////////////
template<typename TRead>
void ReadObjects(JsonValue const& json, std::string_view path, std::span<std::string_view const> fields, std::string& problem, TRead&& read)
{
	if (json.is_array())
	{
		size_t index{ 0 };

		for (JsonValue const& element : json)
		{
			std::string const elementPath{ std::format("{}[{}]", path, index) };

			if (element.is_object())
			{
				CheckFields(element, elementPath, fields, problem);
				read(element, elementPath);
			}
			else
			{
				Fail(problem, elementPath, "must be an object");
			}

			++index;
		}
	}
	else
	{
		Fail(problem, path, "must be an array");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadPort(JsonValue const& object, std::string_view parent, uint16_t& port, std::string& problem)
{
	JsonValue::const_iterator const it{ object.find("port") };

	if (it == object.cend())
	{
		Fail(problem, JoinPath(parent, "port"), "is missing");
	}
	else if (it->is_number_unsigned() && it->get<uint64_t>() >= 1 && it->get<uint64_t>() <= MaxPort)
	{
		port = static_cast<uint16_t>(it->get<uint64_t>());
	}
	else
	{
		Fail(problem, JoinPath(parent, "port"), "must be a whole number from 1 to 65535");
	}
}

//////////////////////////////////////////////////////////////////////////
// Read before anything else: a newer format's fields would otherwise only be reported as unknown.
bool ReadFormat(JsonValue const& root, std::string& problem)
{
	JsonValue::const_iterator const it{ root.find("format") };

	if (it == root.cend())
	{
		Fail(problem, "format", "is missing");
	}
	else if (it->is_number_unsigned() && it->get<uint64_t>() > Format)
	{
		Fail(problem, "format", std::format("is {}, which needs a newer Lookout", it->get<uint64_t>()));
	}
	else if (!it->is_number_unsigned() || it->get<uint64_t>() != Format)
	{
		Fail(problem, "format", std::format("must be {}", Format));
	}

	return problem.empty();
}

//////////////////////////////////////////////////////////////////////////
void ReadMasters(JsonValue const& root, Query::SGameDefinition& game, std::string& problem)
{
	JsonValue::const_iterator const it{ root.find("masters") };

	if (it != root.cend())
	{
		ReadObjects(*it, "masters", MasterFields, problem, [&game, &problem](JsonValue const& object, std::string_view path)
		{
			Query::SMasterEndpoint& master{ game.masters.emplace_back() };

			ReadRequiredString(object, path, "host", master.host, problem);
			ReadPort(object, path, master.port, problem);
		});

		if (game.masters.empty())
		{
			Fail(problem, "masters", "must list at least one master");
		}
	}
	else
	{
		Fail(problem, "masters", "is missing");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadKeys(JsonValue const& root, Query::SServerKeys& keys, std::string& problem)
{
	JsonValue const* const pKeys{ FindObject(root, "keys", true, problem) };

	if (pKeys != nullptr)
	{
		CheckFields(*pKeys, "keys", KeyFields, problem);
		ReadRequiredString(*pKeys, "keys", "hostname", keys.hostname, problem);
		ReadRequiredString(*pKeys, "keys", "map", keys.map, problem);
		ReadRequiredString(*pKeys, "keys", "maxPlayers", keys.maxPlayers, problem);
		ReadRequiredString(*pKeys, "keys", "password", keys.password, problem);

		JsonValue::const_iterator const mods{ pKeys->find("mods") };

		if (mods != pKeys->cend())
		{
			ReadStrings(*mods, "keys.mods", keys.mods, problem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadModes(JsonValue const& root, std::vector<Query::SModeRule>& modes, std::string& problem)
{
	JsonValue::const_iterator const it{ root.find("modes") };

	if (it != root.cend())
	{
		ReadObjects(*it, "modes", ModeFields, problem, [&modes, &problem](JsonValue const& object, std::string_view path)
		{
			Query::SModeRule& mode{ modes.emplace_back() };

			ReadRequiredString(object, path, "key", mode.match.key, problem);
			ReadRequiredString(object, path, "value", mode.match.value, problem);
			ReadRequiredString(object, path, "label", mode.label, problem);
		});
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadForeignServers(JsonValue const& root, std::vector<Query::SKeyMatch>& matches, std::string& problem)
{
	JsonValue::const_iterator const it{ root.find("foreignServers") };

	if (it != root.cend())
	{
		ReadObjects(*it, "foreignServers", MatchFields, problem, [&matches, &problem](JsonValue const& object, std::string_view path)
		{
			Query::SKeyMatch& match{ matches.emplace_back() };

			ReadRequiredString(object, path, "key", match.key, problem);
			ReadRequiredString(object, path, "value", match.value, problem);
		});
	}
}

//////////////////////////////////////////////////////////////////////////
// All or nothing: an install folder without its program would be probed and rejected on every start.
void ReadLaunch(JsonValue const& root, Query::SLaunchHints& launch, std::string& problem)
{
	JsonValue const* const pLaunch{ FindObject(root, "launch", false, problem) };

	if (pLaunch != nullptr)
	{
		CheckFields(*pLaunch, "launch", LaunchFields, problem);
		ReadRequiredStrings(*pLaunch, "launch", "desktopFiles", launch.desktopFiles, problem);
		ReadRequiredString(*pLaunch, "launch", "installDir", launch.installDir, problem);
		ReadRequiredString(*pLaunch, "launch", "program", launch.program, problem);
		ReadRequiredStrings(*pLaunch, "launch", "requiredFiles", launch.requiredFiles, problem);
	}
}

//////////////////////////////////////////////////////////////////////////
// Null, with the problem set, when the protocol is not one of the loaded scripts.
Query::SProtocolDefinition const* ReadProtocol(JsonValue const& root, std::span<Query::SProtocolDefinition const> protocols, Query::EProtocol& protocol,
	std::string& problem)
{
	Query::SProtocolDefinition const* pFound{ nullptr };
	std::string name{};

	ReadRequiredString(root, {}, "protocol", name, problem);

	auto const it{ std::ranges::find(protocols, name, &Query::SProtocolDefinition::name) };

	if (it != protocols.end())
	{
		pFound = &*it;
		protocol = static_cast<Query::EProtocol>(it - protocols.begin());
	}
	else if (!name.empty())
	{
		std::string known{};

		for (Query::SProtocolDefinition const& candidate : protocols)
		{
			known += std::format("{}{}", known.empty() ? "" : ", ", candidate.name);
		}

		Fail(problem, "protocol", std::format("'{}' is not one of {}", name, known));
	}

	return pFound;
}

//////////////////////////////////////////////////////////////////////////
// Checked against what the script declares, so a game cannot pass an option its protocol never reads, or leave out one
// it needs.
void ReadProtocolOptions(JsonValue const& root, Query::SProtocolDefinition const& protocol, std::map<std::string, std::string>& options,
	std::string& problem)
{
	JsonValue const* const pOptions{ FindObject(root, "protocolOptions", false, problem) };

	if (pOptions != nullptr)
	{
		for (auto const& item : pOptions->items())
		{
			std::string const path{ JoinPath("protocolOptions", item.key()) };

			if (std::ranges::contains(protocol.options, item.key(), &Query::SProtocolOption::name))
			{
				ReadString(item.value(), path, options[item.key()], problem);
			}
			else
			{
				Fail(problem, path, std::format("is not an option of the {} protocol", protocol.name));
			}
		}
	}

	for (Query::SProtocolOption const& option : protocol.options)
	{
		if (option.isRequired && !options.contains(option.name))
		{
			Fail(problem, JoinPath("protocolOptions", option.name), std::format("is missing; the {} protocol requires it", protocol.name));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadGame(JsonValue const& root, std::span<Query::SProtocolDefinition const> protocols, Query::SGameDefinition& game, std::string& problem)
{
	CheckFields(root, {}, GameFields, problem);
	ReadRequiredString(root, {}, "name", game.name, problem);

	Query::SProtocolDefinition const* const pProtocol{ ReadProtocol(root, protocols, game.protocol, problem) };

	if (pProtocol != nullptr)
	{
		ReadProtocolOptions(root, *pProtocol, game.protocolOptions, problem);
	}

	ReadName(root, "textStyle", TextStyles, game.textStyle, problem);
	ReadMasters(root, game, problem);
	ReadKeys(root, game.keys, problem);
	ReadModes(root, game.modes, problem);
	ReadForeignServers(root, game.foreignServers, problem);
	ReadLaunch(root, game.launch, problem);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<Query::SGameDefinition, std::string> ReadGameJson(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	Query::SGameDefinition game{};
	std::string problem{};

	if (root.is_discarded())
	{
		problem = "it is not valid JSON";
	}
	else if (!root.is_object())
	{
		problem = "it must hold a JSON object";
	}
	else if (ReadFormat(root, problem))
	{
		ReadGame(root, protocols, game, problem);
	}

	std::expected<Query::SGameDefinition, std::string> result{ std::move(game) };

	if (!problem.empty())
	{
		result = std::unexpected{ std::move(problem) };
	}

	return result;
}
} // namespace Lkt::Games
