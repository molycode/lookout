#include "game_json.hpp"
#include "games/game_fields.hpp"
#include "games/game_format.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
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
constexpr bool IgnoreComments{ false };
constexpr std::string_view CommentPrefix{ "//" };
constexpr uint64_t MaxPort{ 65535 };
// Any larger offset leaves no port that both the query and the join could use.
constexpr int64_t MaxPortOffset{ 65534 };

constexpr std::string_view AddressPlaceholder{ "{address}" };
constexpr std::string_view PasswordPlaceholder{ "{password}" };
constexpr uint64_t MaxPasswordLength{ 1024 };
constexpr size_t MaxPaletteSize{ 256 };

constexpr std::array<std::string_view, 2> EncodingNames{ "ascii7", "utf8OrWindows1252" };
constexpr std::array<std::string_view, 3> ColourCodeNames{ "alphanumeric", "printable", "rgb" };

using enum EGameFieldKind;

constexpr std::array<SGameField, 40> Fields{ {
	{ "", "format", "Format", Version, true, {}, "The version of this format: 1." },
	{ "", "name", "Name", Text, true, {}, "The game's name in the game list." },
	{ "", "protocol", "Protocol", Protocol, true, {}, "The protocol script that queries the game's masters and servers, by name." },
	{ "", "protocolOptions", "Protocol options", ProtocolOptions, false, {},
		"The protocol's options, each a string; optional unless the protocol requires one." },
	{ "", "queryPortOffset", "Query port offset", Number, false, {},
		"Optional: the query port less the join port, for servers that answer queries on another port." },
	{ "", "masters", "Masters", GroupList, true, {}, "The master servers that list the game's servers; at least one." },
	{ "masters[]", "host", "Host", Text, true, {}, "The master's host name or IP address." },
	{ "masters[]", "port", "Port", Number, true, {}, "The master's port, 1 to 65535." },
	{ "", "keys", "Server rules", Group, true, {}, "Which of a server's rules hold what the list shows." },
	{ "keys", "hostname", "Name rule", Text, true, {}, "The rule holding the server's name." },
	{ "keys", "map", "Map rule", Text, true, {}, "The rule holding the map." },
	{ "keys", "numPlayers", "Players rule", Text, false, {},
		"Optional: the rule holding the number of players; without it, the listed players are counted." },
	{ "keys", "maxPlayers", "Max players rule", Text, true, {}, "The rule holding how many players fit." },
	{ "keys", "password", "Password rule", Text, true, {}, "The rule whose lowest bit says joining needs a password." },
	{ "keys", "mods", "Mod rules", TextList, false, {}, "Optional: the rules that may name the mod; the first a server has is shown." },
	{ "", "modes", "Modes", GroupList, false, {}, "Optional: the game's modes; the first that matches a server's rules is shown." },
	{ "modes[]", "key", "Rule", Text, true, {}, "The rule to look at." },
	{ "modes[]", "value", "Value", Text, true, {}, "The value the rule holds in this mode." },
	{ "modes[]", "label", "Label", Text, true, {}, "The mode's name in the list." },
	{ "", "foreignServers", "Other games' servers", GroupList, false, {},
		"Optional: rule values that mark another game's servers on the same masters, which are left out." },
	{ "foreignServers[]", "key", "Rule", Text, true, {}, "The rule to look at." },
	{ "foreignServers[]", "value", "Value", Text, true, {}, "The value that marks another game's server." },
	{ "", "text", "Text", Group, true, {}, "How names and maps are encoded and coloured." },
	{ "text", "encoding", "Encoding", Choice, true, EncodingNames,
		"ascii7 (the high bit is dropped) or utf8OrWindows1252 (UTF-8 where valid, else Windows-1252)." },
	{ "text", "colourCodes", "Colour codes", Group, false, {}, "Optional: the colour codes in names." },
	{ "text.colourCodes", "escape", "Escape", Characters, true, {}, "The ASCII character that starts a colour code, such as \"^\"." },
	{ "text.colourCodes", "codes", "Codes", Choice, true, ColourCodeNames,
		"What follows the escape: alphanumeric (a letter or digit), printable (any printable character but the escape) "
		"or rgb (three bytes of red, green and blue)." },
	{ "text.colourCodes", "palette", "Palette", TextList, true, {},
		"The colours the codes pick, each \"#rrggbb\", a power of two of them up to 256; not for rgb." },
	{ "", "join", "Joining", Group, true, {}, "The arguments the game is started with to join a server." },
	{ "join", "arguments", "Arguments", TextList, true, {}, "To join a server; {address} is its address." },
	{ "join", "passwordArguments", "Arguments with a password", TextList, true, {},
		"To join a server with a password; {address} is its address and {password} the password." },
	{ "join", "password", "Passwords", Group, true, {}, "The passwords the game can take." },
	{ "join.password", "maxLength", "Longest", Number, true, {}, "The longest password, 1 to 1024 characters." },
	{ "join.password", "refusedCharacters", "Refused characters", Characters, false, {}, "Optional: characters a password cannot hold." },
	{ "join.password", "refusedSequences", "Refused sequences", TextList, false, {}, "Optional: character sequences a password cannot hold." },
	{ "", "launch", "Installed game", Group, false, {}, "Optional: how to find the installed game, to join with it." },
	{ "launch", "desktopFiles", "Desktop files", TextList, true, {}, "Desktop entries that start the game, by file name, in order of preference." },
	{ "launch", "installDir", "Install folder", Text, true, {}, "The game's usual install folder, under the home folder." },
	{ "launch", "program", "Program", Text, true, {}, "The program to run, inside the install folder." },
	{ "launch", "requiredFiles", "Required files", TextList, true, {}, "Files inside the install folder that show the game is there." }
} };

constexpr std::array<std::pair<std::string_view, Query::ETextEncoding>, 2> Encodings{ {
	{ EncodingNames[0], Query::ETextEncoding::Ascii7 },
	{ EncodingNames[1], Query::ETextEncoding::Utf8OrWindows1252 }
} };

constexpr std::array<std::pair<std::string_view, Query::EColorCodes>, 3> ColourCodes{ {
	{ ColourCodeNames[0], Query::EColorCodes::Alphanumeric },
	{ ColourCodeNames[1], Query::EColorCodes::Printable },
	{ ColourCodeNames[2], Query::EColorCodes::Rgb }
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
bool IsComment(std::string_view key)
{
	return key.starts_with(CommentPrefix);
}

//////////////////////////////////////////////////////////////////////////
// Its field must be beside it, so a comment left behind by a removed or renamed field is caught.
void CheckComment(JsonValue const& object, std::string_view path, std::string_view key, JsonValue const& comment, std::string& problem)
{
	std::string_view const field{ key.substr(CommentPrefix.size()) };
	std::string text{};

	if (object.find(field) == object.cend())
	{
		Fail(problem, JoinPath(path, key), std::format("is a comment on '{}', which is not in this object", field));
	}

	ReadString(comment, JoinPath(path, key), text, problem);
}

//////////////////////////////////////////////////////////////////////////
// "masters[2]" is held by "masters[]": the table names an array's objects without an index.
std::string ToParent(std::string_view path)
{
	std::string parent{};
	bool isIndex{ false };

	for (char const c : path)
	{
		isIndex = (c == '[') || (isIndex && c != ']');

		if (!isIndex || c == '[')
		{
			parent += c;
		}
	}

	return parent;
}

//////////////////////////////////////////////////////////////////////////
// A field format 1 does not know is a typo or a later format's, so it is never skipped silently.
void CheckFields(JsonValue const& object, std::string_view path, std::string& problem)
{
	std::string const parent{ ToParent(path) };

	for (auto const& item : object.items())
	{
		std::string_view const key{ item.key() };
		bool const isField{ std::ranges::any_of(Fields, [&parent, key](SGameField const& field) { return field.parent == parent && field.name == key; }) };

		if (IsComment(key))
		{
			CheckComment(object, path, key, item.value(), problem);
		}
		else if (!isField)
		{
			Fail(problem, JoinPath(path, key), "is not a field of format 1");
		}
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
void ReadName(JsonValue const& object, std::string_view parent, std::string_view key, std::array<std::pair<std::string_view, TEnum>, NumNames> const& names,
	TEnum& value, std::string& problem)
{
	std::string name{};

	ReadRequiredString(object, parent, key, name, problem);

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

		Fail(problem, JoinPath(parent, key), std::format("'{}' is not one of {}", name, known));
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
void ReadObjects(JsonValue const& json, std::string_view path, std::string& problem, TRead&& read)
{
	if (json.is_array())
	{
		size_t index{ 0 };

		for (JsonValue const& element : json)
		{
			std::string const elementPath{ std::format("{}[{}]", path, index) };

			if (element.is_object())
			{
				CheckFields(element, elementPath, problem);
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
	else if (it->is_number_unsigned() && it->get<uint64_t>() > GameFormat)
	{
		Fail(problem, "format", std::format("is {}, which needs a newer Lookout", it->get<uint64_t>()));
	}
	else if (!it->is_number_unsigned() || it->get<uint64_t>() != GameFormat)
	{
		Fail(problem, "format", std::format("must be {}", GameFormat));
	}

	return problem.empty();
}

//////////////////////////////////////////////////////////////////////////
void ReadMasters(JsonValue const& root, Query::SGameDefinition& game, std::string& problem)
{
	JsonValue::const_iterator const it{ root.find("masters") };

	if (it != root.cend())
	{
		ReadObjects(*it, "masters", problem, [&game, &problem](JsonValue const& object, std::string_view path)
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
		CheckFields(*pKeys, "keys", problem);
		ReadRequiredString(*pKeys, "keys", "hostname", keys.hostname, problem);
		ReadRequiredString(*pKeys, "keys", "map", keys.map, problem);
		ReadRequiredString(*pKeys, "keys", "maxPlayers", keys.maxPlayers, problem);
		ReadRequiredString(*pKeys, "keys", "password", keys.password, problem);

		JsonValue::const_iterator const numPlayers{ pKeys->find("numPlayers") };
		JsonValue::const_iterator const mods{ pKeys->find("mods") };

		if (numPlayers != pKeys->cend())
		{
			ReadString(*numPlayers, "keys.numPlayers", keys.numPlayers, problem);
		}

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
		ReadObjects(*it, "modes", problem, [&modes, &problem](JsonValue const& object, std::string_view path)
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
		ReadObjects(*it, "foreignServers", problem, [&matches, &problem](JsonValue const& object, std::string_view path)
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
		CheckFields(*pLaunch, "launch", problem);
		ReadRequiredStrings(*pLaunch, "launch", "desktopFiles", launch.desktopFiles, problem);
		ReadRequiredString(*pLaunch, "launch", "installDir", launch.installDir, problem);
		ReadRequiredString(*pLaunch, "launch", "program", launch.program, problem);
		ReadRequiredStrings(*pLaunch, "launch", "requiredFiles", launch.requiredFiles, problem);
	}
}

//////////////////////////////////////////////////////////////////////////
// "#rrggbb"
bool ReadColour(JsonValue const& json, Tge::SColor& colour)
{
	constexpr size_t Length{ 7 };
	constexpr int Base{ 16 };
	bool isValid{ json.is_string() && json.get_ref<std::string const&>().size() == Length && json.get_ref<std::string const&>().front() == '#' };
	uint32_t value{ 0 };

	if (isValid)
	{
		std::string const& text{ json.get_ref<std::string const&>() };
		std::from_chars_result const result{ std::from_chars(text.data() + 1, text.data() + text.size(), value, Base) };

		isValid = result.ec == std::errc{} && result.ptr == text.data() + text.size();
	}

	if (isValid)
	{
		colour = Tge::SColor{ static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value) };
	}

	return isValid;
}

//////////////////////////////////////////////////////////////////////////
// A power of two, so a code's palette index is a mask, as the games compute it.
void ReadPalette(JsonValue const& codes, std::vector<Tge::SColor>& palette, std::string& problem)
{
	constexpr std::string_view Path{ "text.colourCodes.palette" };
	JsonValue::const_iterator const it{ codes.find("palette") };

	if (it == codes.cend())
	{
		Fail(problem, Path, "is missing");
	}
	else if (!it->is_array() || it->empty() || it->size() > MaxPaletteSize || !std::has_single_bit(it->size()))
	{
		Fail(problem, Path, std::format("must list a power of two of colours, from 1 to {}", MaxPaletteSize));
	}
	else
	{
		size_t index{ 0 };

		for (JsonValue const& entry : *it)
		{
			if (!ReadColour(entry, palette.emplace_back()))
			{
				Fail(problem, std::format("{}[{}]", Path, index), "must be a colour written #rrggbb");
			}

			++index;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadColourCodes(JsonValue const& text, Query::STextStyle& style, std::string& problem)
{
	constexpr std::string_view Path{ "text.colourCodes" };
	JsonValue::const_iterator const it{ text.find("colourCodes") };

	if (it != text.cend() && !it->is_object())
	{
		Fail(problem, Path, "must be an object");
	}
	else if (it != text.cend())
	{
		std::string escape{};

		CheckFields(*it, Path, problem);
		ReadRequiredString(*it, Path, "escape", escape, problem);

		if (escape.size() == 1 && static_cast<unsigned char>(escape.front()) < 0x80)
		{
			style.escape = escape.front();
		}
		else if (!escape.empty())
		{
			Fail(problem, JoinPath(Path, "escape"), "must be one ASCII character");
		}

		ReadName(*it, Path, "codes", ColourCodes, style.codes, problem);

		if (style.codes == Query::EColorCodes::Rgb && it->contains("palette"))
		{
			Fail(problem, JoinPath(Path, "palette"), "is not used by rgb codes, which carry their colour");
		}
		else if (style.codes == Query::EColorCodes::Alphanumeric || style.codes == Query::EColorCodes::Printable)
		{
			ReadPalette(*it, style.palette, problem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadText(JsonValue const& root, Query::STextStyle& style, std::string& problem)
{
	JsonValue const* const pText{ FindObject(root, "text", true, problem) };

	if (pText != nullptr)
	{
		CheckFields(*pText, "text", problem);
		ReadName(*pText, "text", "encoding", Encodings, style.encoding, problem);
		ReadColourCodes(*pText, style, problem);
	}
}

//////////////////////////////////////////////////////////////////////////
// A brace that starts neither placeholder is a typo, which would otherwise reach the game unfilled.
void CheckPlaceholders(std::vector<std::string> const& arguments, std::string_view path, bool needsPassword, std::string& problem)
{
	bool hasAddress{ false };
	bool hasPassword{ false };

	for (size_t index{ 0 }; index < arguments.size(); ++index)
	{
		std::string_view const argument{ arguments[index] };

		for (size_t at{ argument.find('{') }; at != std::string_view::npos; at = argument.find('{', at + 1))
		{
			std::string_view const rest{ argument.substr(at) };

			hasAddress = hasAddress || rest.starts_with(AddressPlaceholder);
			hasPassword = hasPassword || rest.starts_with(PasswordPlaceholder);

			if (!rest.starts_with(AddressPlaceholder) && !rest.starts_with(PasswordPlaceholder))
			{
				Fail(problem, std::format("{}[{}]", path, index), "holds a placeholder other than {address} and {password}");
			}
		}
	}

	if (!hasAddress)
	{
		Fail(problem, path, "must hold {address}");
	}
	else if (hasPassword != needsPassword)
	{
		Fail(problem, path, needsPassword ? "must hold {password}" : "must not hold {password}");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadPasswordRules(JsonValue const& join, Query::SPasswordRules& rules, std::string& problem)
{
	constexpr std::string_view Path{ "join.password" };
	JsonValue::const_iterator const it{ join.find("password") };

	if (it == join.cend() || !it->is_object())
	{
		Fail(problem, Path, "must be an object");
	}
	else
	{
		JsonValue::const_iterator const maxLength{ it->find("maxLength") };
		JsonValue::const_iterator const refusedCharacters{ it->find("refusedCharacters") };
		JsonValue::const_iterator const refusedSequences{ it->find("refusedSequences") };

		CheckFields(*it, Path, problem);

		if (maxLength != it->cend() && maxLength->is_number_unsigned() && maxLength->get<uint64_t>() >= 1 && maxLength->get<uint64_t>() <= MaxPasswordLength)
		{
			rules.maxLength = static_cast<uint32_t>(maxLength->get<uint64_t>());
		}
		else
		{
			Fail(problem, JoinPath(Path, "maxLength"), std::format("must be a whole number from 1 to {}", MaxPasswordLength));
		}

		if (refusedCharacters != it->cend())
		{
			ReadString(*refusedCharacters, JoinPath(Path, "refusedCharacters"), rules.refusedCharacters, problem);
		}

		if (refusedSequences != it->cend())
		{
			ReadStrings(*refusedSequences, JoinPath(Path, "refusedSequences"), rules.refusedSequences, problem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadJoin(JsonValue const& root, Query::SJoinCommand& join, std::string& problem)
{
	JsonValue const* const pJoin{ FindObject(root, "join", true, problem) };

	if (pJoin != nullptr)
	{
		CheckFields(*pJoin, "join", problem);
		ReadRequiredStrings(*pJoin, "join", "arguments", join.arguments, problem);
		ReadRequiredStrings(*pJoin, "join", "passwordArguments", join.passwordArguments, problem);
		CheckPlaceholders(join.arguments, "join.arguments", false, problem);
		CheckPlaceholders(join.passwordArguments, "join.passwordArguments", true, problem);
		ReadPasswordRules(*pJoin, join.password, problem);
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

			if (IsComment(item.key()))
			{
				CheckComment(*pOptions, "protocolOptions", item.key(), item.value(), problem);
			}
			else if (std::ranges::contains(protocol.options, item.key(), &Query::SProtocolOption::name))
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
void ReadQueryPortOffset(JsonValue const& root, int32_t& offset, std::string& problem)
{
	JsonValue::const_iterator const it{ root.find("queryPortOffset") };

	if (it != root.cend())
	{
		bool const isValid{ (it->is_number_unsigned() && it->get<uint64_t>() <= static_cast<uint64_t>(MaxPortOffset))
			|| (it->is_number_integer() && !it->is_number_unsigned() && it->get<int64_t>() >= -MaxPortOffset) };

		if (isValid)
		{
			offset = static_cast<int32_t>(it->get<int64_t>());
		}
		else
		{
			Fail(problem, "queryPortOffset", std::format("must be a whole number from {} to {}", -MaxPortOffset, MaxPortOffset));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadGame(JsonValue const& root, std::span<Query::SProtocolDefinition const> protocols, Query::SGameDefinition& game, std::string& problem)
{
	CheckFields(root, {}, problem);
	ReadRequiredString(root, {}, "name", game.name, problem);

	Query::SProtocolDefinition const* const pProtocol{ ReadProtocol(root, protocols, game.protocol, problem) };

	if (pProtocol != nullptr)
	{
		ReadProtocolOptions(root, *pProtocol, game.protocolOptions, problem);
	}

	ReadQueryPortOffset(root, game.queryPortOffset, problem);
	ReadMasters(root, game, problem);
	ReadKeys(root, game.keys, problem);
	ReadModes(root, game.modes, problem);
	ReadForeignServers(root, game.foreignServers, problem);
	ReadText(root, game.text, problem);
	ReadJoin(root, game.join, problem);
	ReadLaunch(root, game.launch, problem);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::span<SGameField const> GetGameFields()
{
	return Fields;
}

//////////////////////////////////////////////////////////////////////////
std::expected<Query::SGameDefinition, std::string> ReadGameJson(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	Query::SGameDefinition game{};
	std::string problem{};

	if (root.is_discarded())
	{
		problem = std::format("it is not valid JSON: {}", Json::DescribeSyntaxError(text, IgnoreComments));
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
