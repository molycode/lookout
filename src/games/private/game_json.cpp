#include "game_json.hpp"
#include "format_problem.hpp"
#include "games/game_fields.hpp"
#include "games/game_format.hpp"
#include "games/read_game_fields.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
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

constexpr std::string_view LaunchPath{ "launch" };
constexpr std::string_view LinuxPath{ "launch.linux" };
constexpr std::string_view WindowsPath{ "launch.windows" };
constexpr std::array<std::string_view, 3> InstallFields{ "installDir", "program", "requiredFiles" };
constexpr std::array<std::string_view, 2> ProgramExtensions{ ".exe", ".com" };

constexpr std::array<std::pair<std::string_view, Query::EInstallDirBase>, 5> InstallDirBases{ {
	{ "{programFiles}", Query::EInstallDirBase::ProgramFiles },
	{ "{programFilesX86}", Query::EInstallDirBase::ProgramFilesX86 },
	{ "{appData}", Query::EInstallDirBase::AppData },
	{ "{localAppData}", Query::EInstallDirBase::LocalAppData },
	{ "{systemDrive}", Query::EInstallDirBase::SystemDrive }
} };

using enum EGameFieldKind;

constexpr std::array<SGameField, 48> Fields{ {
	{ "", "format", "Format", Version, true, {}, "The format of this file: 1, or 3 for launch facts per platform." },
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
		"or rgb (red, green and blue, a byte each, or a character each where the text is UTF-8)." },
	{ "text.colourCodes", "palette", "Palette", TextList, true, {},
		"The colours the codes pick, each \"#rrggbb\", a power of two of them up to 256; not for rgb." },
	{ "text.colourCodes", "hexCodes", "Hex codes", TextList, false, {},
		"Optional: codes that carry their colour in hex, each what follows the escape, then RGB or RRGGBB, such as "
		"\"xRGB\" or \"#RRGGBB\"; tried before the palette, not for rgb." },
	{ "", "join", "Joining", Group, true, {}, "The arguments the game is started with to join a server." },
	{ "join", "arguments", "Arguments", TextList, true, {}, "To join a server; {address} is its address." },
	{ "join", "passwordArguments", "Arguments with a password", TextList, true, {},
		"To join a server with a password; {address} is its address and {password} the password." },
	{ "join", "password", "Passwords", Group, true, {}, "The passwords the game can take." },
	{ "join.password", "maxLength", "Longest", Number, true, {}, "The longest password, 1 to 1024 characters." },
	{ "join.password", "refusedCharacters", "Refused characters", Characters, false, {}, "Optional: characters a password cannot hold." },
	{ "join.password", "refusedSequences", "Refused sequences", TextList, false, {}, "Optional: character sequences a password cannot hold." },
	{ "", "launch", "Installed game", Group, false, {}, "Optional: how Lookout finds the installed game on Linux, on Windows or both, to join with it." },
	{ "launch", "linux", "Linux", Group, false, {},
		"Optional: the game on Linux, found through its desktop entries, its install folder under the home folder, or both." },
	{ "launch.linux", "desktopFiles", "Desktop files", TextList, false, {}, "Optional: desktop entries that start the game, by file name, in order of preference." },
	{ "launch.linux", "installDir", "Install folder", Text, false, {},
		"Optional, with Program and Required files: the game's usual install folder, under the home folder." },
	{ "launch.linux", "program", "Program", Text, false, {}, "Optional, with Install folder and Required files: the program to run, inside the install folder." },
	{ "launch.linux", "requiredFiles", "Required files", TextList, false, {},
		"Optional, with Install folder and Program: files inside the install folder that show the game is there." },
	{ "launch", "windows", "Windows", Group, false, {},
		"Optional: the game on Windows, found through Steam, GOG or its usual install folders; at least one of the three." },
	{ "launch.windows", "steamAppId", "Steam app id", Number, false, {}, "Optional: the game's Steam app id, such as 38430; Lookout looks in every Steam library." },
	{ "launch.windows", "gogId", "GOG game id", Number, false, {}, "Optional: the game's GOG id, as GOG's installers register it." },
	{ "launch.windows", "installDirs", "Install folders", TextList, false, {},
		"Optional: usual install folders, each {programFiles}, {programFilesX86}, {appData}, {localAppData} or {systemDrive}, then / and the folder, such as "
		"{programFilesX86}/Kingpin." },
	{ "launch.windows", "program", "Program", Text, true, {}, "The program to run, an .exe or .com inside the install folder, with / between folders." },
	{ "launch.windows", "requiredFiles", "Required files", TextList, true, {},
		"Files inside the install folder that show the game is there, with / between folders." }
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
void Fail(SFieldProblem& problem, std::string_view path, std::string_view reason)
{
	if (problem.reason.empty())
	{
		problem = SFieldProblem{ std::string{ path }, std::string{ reason } };
	}
}

//////////////////////////////////////////////////////////////////////////
// Every string names a rule, a host or a path, so an empty one or a NUL is always a mistake.
void ReadString(JsonValue const& json, std::string_view path, std::string& value, SFieldProblem& problem)
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
void CheckComment(JsonValue const& object, std::string_view path, std::string_view key, JsonValue const& comment, SFieldProblem& problem)
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
// A field the file's format does not know is a typo or a later format's, so it is never skipped silently.
void CheckFieldsAs(JsonValue const& object, std::string_view path, std::string_view schemaParent, uint64_t format, SFieldProblem& problem)
{
	for (auto const& item : object.items())
	{
		std::string_view const key{ item.key() };
		bool const isField{ std::ranges::any_of(Fields, [schemaParent, key](SGameField const& field) { return field.parent == schemaParent && field.name == key; }) };

		if (IsComment(key))
		{
			CheckComment(object, path, key, item.value(), problem);
		}
		else if (!isField)
		{
			Fail(problem, JoinPath(path, key), std::format("is not a field of format {}", format));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CheckFields(JsonValue const& object, std::string_view path, uint64_t format, SFieldProblem& problem)
{
	CheckFieldsAs(object, path, ToParent(path), format, problem);
}

//////////////////////////////////////////////////////////////////////////
void ReadStrings(JsonValue const& json, std::string_view path, std::vector<std::string>& values, SFieldProblem& problem)
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
void ReadRequiredString(JsonValue const& object, std::string_view parent, std::string_view key, std::string& value, SFieldProblem& problem)
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
void ReadRequiredStrings(JsonValue const& object, std::string_view parent, std::string_view key, std::vector<std::string>& values, SFieldProblem& problem)
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
	TEnum& value, SFieldProblem& problem)
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
JsonValue const* FindObject(JsonValue const& object, std::string_view parent, std::string_view key, bool isRequired, SFieldProblem& problem)
{
	JsonValue const* pFound{ nullptr };
	JsonValue::const_iterator const it{ object.find(key) };

	if (it != object.cend() && it->is_object())
	{
		pFound = &*it;
	}
	else if (it != object.cend())
	{
		Fail(problem, JoinPath(parent, key), "must be an object");
	}
	else if (isRequired)
	{
		Fail(problem, JoinPath(parent, key), "is missing");
	}

	return pFound;
}

//////////////////////////////////////////////////////////////////////////
template<typename TRead>
void ReadObjects(JsonValue const& json, std::string_view path, uint64_t format, SFieldProblem& problem, TRead&& read)
{
	if (json.is_array())
	{
		size_t index{ 0 };

		for (JsonValue const& element : json)
		{
			std::string const elementPath{ std::format("{}[{}]", path, index) };

			if (element.is_object())
			{
				CheckFields(element, elementPath, format, problem);
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
void ReadPort(JsonValue const& object, std::string_view parent, uint16_t& port, SFieldProblem& problem)
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
std::optional<uint64_t> ReadFormat(JsonValue const& root, SFieldProblem& problem)
{
	JsonValue::const_iterator const it{ root.find("format") };
	std::optional<uint64_t> format{};

	if (it == root.cend())
	{
		Fail(problem, "format", "is missing");
	}
	else if (!it->is_number_unsigned())
	{
		Fail(problem, "format", std::format("must be {} or {}", FirstGameFormat, GameFormat));
	}
	else if (std::optional<std::string> const reason{ DescribeUnreadableFormat(it->get<uint64_t>()) }; reason.has_value())
	{
		Fail(problem, "format", *reason);
	}
	else
	{
		format = it->get<uint64_t>();
	}

	return format;
}

//////////////////////////////////////////////////////////////////////////
void ReadKeys(JsonValue const& root, Query::SServerKeys& keys, uint64_t format, SFieldProblem& problem)
{
	JsonValue const* const pKeys{ FindObject(root, {}, "keys", true, problem) };

	if (pKeys != nullptr)
	{
		CheckFields(*pKeys, "keys", format, problem);
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
void ReadModes(JsonValue const& root, std::vector<Query::SModeRule>& modes, uint64_t format, SFieldProblem& problem)
{
	JsonValue::const_iterator const it{ root.find("modes") };

	if (it != root.cend())
	{
		ReadObjects(*it, "modes", format, problem, [&modes, &problem](JsonValue const& object, std::string_view path)
		{
			Query::SModeRule& mode{ modes.emplace_back() };

			ReadRequiredString(object, path, "key", mode.match.key, problem);
			ReadRequiredString(object, path, "value", mode.match.value, problem);
			ReadRequiredString(object, path, "label", mode.label, problem);
		});
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadForeignServers(JsonValue const& root, std::vector<Query::SKeyMatch>& matches, uint64_t format, SFieldProblem& problem)
{
	JsonValue::const_iterator const it{ root.find("foreignServers") };

	if (it != root.cend())
	{
		ReadObjects(*it, "foreignServers", format, problem, [&matches, &problem](JsonValue const& object, std::string_view path)
		{
			Query::SKeyMatch& match{ matches.emplace_back() };

			ReadRequiredString(object, path, "key", match.key, problem);
			ReadRequiredString(object, path, "value", match.value, problem);
		});
	}
}

//////////////////////////////////////////////////////////////////////////
// Inside the install folder: no drive, root, backslash, or "." and ".." that would leave it.
bool IsInsidePath(std::string_view path)
{
	bool isInside{ !path.empty() && !path.starts_with('/') && !path.contains('\\') && !path.contains(':') };

	for (size_t start{ 0 }; isInside && start <= path.size(); )
	{
		size_t const end{ std::min(path.find('/', start), path.size()) };
		std::string_view const segment{ path.substr(start, end - start) };

		isInside = !segment.empty() && segment != "." && segment != "..";
		start = end + 1;
	}

	return isInside;
}

//////////////////////////////////////////////////////////////////////////
bool HasProgramExtension(std::string_view program)
{
	return std::ranges::any_of(ProgramExtensions, [program](std::string_view extension)
	{
		return program.size() > extension.size() && std::ranges::equal(program.substr(program.size() - extension.size()), extension, [](char lhs, char rhs)
		{
			return std::tolower(static_cast<unsigned char>(lhs)) == rhs;
		});
	});
}

//////////////////////////////////////////////////////////////////////////
// A whole number from 1 to max, or nothing with the problem set.
std::optional<uint64_t> ReadId(JsonValue const& object, std::string_view key, uint64_t max, std::string_view range, SFieldProblem& problem)
{
	JsonValue::const_iterator const it{ object.find(key) };
	std::optional<uint64_t> id{};

	if (it != object.cend() && it->is_number_unsigned() && it->get<uint64_t>() >= 1 && it->get<uint64_t>() <= max)
	{
		id = it->get<uint64_t>();
	}
	else if (it != object.cend())
	{
		Fail(problem, JoinPath(WindowsPath, key), std::format("must be a whole number {}", range));
	}

	return id;
}

//////////////////////////////////////////////////////////////////////////
void ReadInstallDirs(JsonValue const& json, std::vector<Query::SInstallDirCandidate>& installDirs, SFieldProblem& problem)
{
	std::string const path{ JoinPath(WindowsPath, "installDirs") };
	std::vector<std::string> entries{};

	ReadStrings(json, path, entries, problem);

	for (size_t index{ 0 }; index < entries.size(); ++index)
	{
		std::string_view const entry{ entries[index] };
		auto const base{ std::ranges::find_if(InstallDirBases, [entry](std::pair<std::string_view, Query::EInstallDirBase> const& candidate)
		{
			return entry.starts_with(candidate.first) && entry.substr(candidate.first.size()).starts_with('/');
		}) };
		std::string_view const folder{ (base != InstallDirBases.end()) ? entry.substr(base->first.size() + 1) : std::string_view{} };

		if (base == InstallDirBases.end())
		{
			Fail(problem, std::format("{}[{}]", path, index), "must start with {programFiles}, {programFilesX86}, {appData}, {localAppData} or {systemDrive}, then /");
		}
		else if (!IsInsidePath(folder))
		{
			Fail(problem, std::format("{}[{}]", path, index), "must name a folder after its placeholder, with / between folders and no . or ..");
		}
		else
		{
			installDirs.emplace_back(base->second, std::string{ folder });
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Desktop entries may stand alone; an install folder needs its program and the files that show the game is there.
void ReadLinuxLaunch(JsonValue const& object, Query::SLinuxLaunchHints& facts, uint64_t format, SFieldProblem& problem)
{
	JsonValue::const_iterator const desktopFiles{ object.find("desktopFiles") };
	bool const hasInstallFolder{ std::ranges::any_of(InstallFields, [&object](std::string_view key) { return object.contains(key); }) };

	CheckFields(object, LinuxPath, format, problem);

	if (desktopFiles != object.cend())
	{
		ReadStrings(*desktopFiles, JoinPath(LinuxPath, "desktopFiles"), facts.desktopFiles, problem);
	}

	for (std::string_view const key : InstallFields)
	{
		if (hasInstallFolder && !object.contains(key))
		{
			Fail(problem, JoinPath(LinuxPath, key), "is missing; installDir, program and requiredFiles go together");
		}
	}

	if (hasInstallFolder)
	{
		ReadRequiredString(object, LinuxPath, "installDir", facts.installDir, problem);
		ReadRequiredString(object, LinuxPath, "program", facts.program, problem);
		ReadRequiredStrings(object, LinuxPath, "requiredFiles", facts.requiredFiles, problem);
	}
	else if (desktopFiles == object.cend())
	{
		Fail(problem, LinuxPath, "must hold desktopFiles, or installDir with program and requiredFiles");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadWindowsLaunch(JsonValue const& object, Query::SWindowsLaunchHints& facts, uint64_t format, SFieldProblem& problem)
{
	JsonValue::const_iterator const installDirs{ object.find("installDirs") };

	CheckFields(object, WindowsPath, format, problem);

	std::optional<uint64_t> const steamAppId{ ReadId(object, "steamAppId", std::numeric_limits<uint32_t>::max(), "from 1 to 4294967295", problem) };
	std::optional<uint64_t> const gogId{ ReadId(object, "gogId", std::numeric_limits<uint64_t>::max(), "from 1", problem) };

	facts.steamAppId = static_cast<uint32_t>(steamAppId.value_or(0));
	facts.gogId = gogId.value_or(0);

	if (installDirs != object.cend())
	{
		ReadInstallDirs(*installDirs, facts.installDirs, problem);
	}

	ReadRequiredString(object, WindowsPath, "program", facts.program, problem);
	ReadRequiredStrings(object, WindowsPath, "requiredFiles", facts.requiredFiles, problem);

	if (!facts.program.empty() && (!IsInsidePath(facts.program) || !HasProgramExtension(facts.program)))
	{
		Fail(problem, JoinPath(WindowsPath, "program"), "must be an .exe or .com inside the install folder, with / between folders");
	}

	for (size_t index{ 0 }; index < facts.requiredFiles.size(); ++index)
	{
		if (!IsInsidePath(facts.requiredFiles[index]))
		{
			Fail(problem, std::format("{}.requiredFiles[{}]", WindowsPath, index), "must be a path inside the install folder, with / between folders");
		}
	}

	if (!steamAppId.has_value() && !gogId.has_value() && facts.installDirs.empty())
	{
		Fail(problem, WindowsPath, "must name steamAppId, gogId or installDirs");
	}
}

//////////////////////////////////////////////////////////////////////////
// Format 1 holds Linux's facts in launch itself, all four or none; format 3 gives each platform an object of its own.
void ReadLaunch(JsonValue const& root, Query::SLaunchHints& launch, uint64_t format, SFieldProblem& problem)
{
	JsonValue const* const pLaunch{ FindObject(root, {}, LaunchPath, false, problem) };

	if (pLaunch != nullptr && format == FirstGameFormat)
	{
		CheckFieldsAs(*pLaunch, LaunchPath, LinuxPath, format, problem);
		ReadRequiredStrings(*pLaunch, LaunchPath, "desktopFiles", launch.onLinux.desktopFiles, problem);
		ReadRequiredString(*pLaunch, LaunchPath, "installDir", launch.onLinux.installDir, problem);
		ReadRequiredString(*pLaunch, LaunchPath, "program", launch.onLinux.program, problem);
		ReadRequiredStrings(*pLaunch, LaunchPath, "requiredFiles", launch.onLinux.requiredFiles, problem);
	}
	else if (pLaunch != nullptr)
	{
		CheckFields(*pLaunch, LaunchPath, format, problem);

		JsonValue const* const pLinux{ FindObject(*pLaunch, LaunchPath, "linux", false, problem) };
		JsonValue const* const pWindows{ FindObject(*pLaunch, LaunchPath, "windows", false, problem) };

		if (pLinux != nullptr)
		{
			ReadLinuxLaunch(*pLinux, launch.onLinux, format, problem);
		}

		if (pWindows != nullptr)
		{
			ReadWindowsLaunch(*pWindows, launch.onWindows, format, problem);
		}

		if (!pLaunch->contains("linux") && !pLaunch->contains("windows"))
		{
			Fail(problem, LaunchPath, "must hold linux, windows or both");
		}
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
void ReadPalette(JsonValue const& codes, std::vector<Tge::SColor>& palette, SFieldProblem& problem)
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
void ReadHexCodes(JsonValue const& json, char escape, std::vector<Query::SHexColorCode>& hexCodes, SFieldProblem& problem)
{
	constexpr std::string_view Path{ "text.colourCodes.hexCodes" };
	constexpr std::string_view ShortDigits{ "RGB" };
	constexpr std::string_view LongDigits{ "RRGGBB" };
	std::vector<std::string> forms{};

	ReadStrings(json, Path, forms, problem);

	for (size_t index{ 0 }; index < forms.size(); ++index)
	{
		std::string_view const form{ forms[index] };
		size_t const numDigits{ form.ends_with(LongDigits) ? LongDigits.size() : (form.ends_with(ShortDigits) ? ShortDigits.size() : 0) };
		std::string_view const prefix{ form.substr(0, form.size() - numDigits) };
		bool const isPrintable{ std::ranges::all_of(prefix, [escape](char c) { return c > ' ' && c < '\x7F' && c != escape; }) };

		if (numDigits != 0 && !prefix.empty() && isPrintable)
		{
			hexCodes.emplace_back(std::string{ prefix }, static_cast<uint8_t>(numDigits));
		}
		else
		{
			Fail(problem, std::format("{}[{}]", Path, index), "must be printable characters other than the escape, then RGB or RRGGBB");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadColourCodes(JsonValue const& text, Query::STextStyle& style, uint64_t format, SFieldProblem& problem)
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

		CheckFields(*it, Path, format, problem);
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

		JsonValue::const_iterator const hexCodes{ it->find("hexCodes") };

		if (style.codes == Query::EColorCodes::Rgb && hexCodes != it->cend())
		{
			Fail(problem, JoinPath(Path, "hexCodes"), "is not used by rgb codes");
		}
		else if (hexCodes != it->cend())
		{
			ReadHexCodes(*hexCodes, style.escape, style.hexCodes, problem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadText(JsonValue const& root, Query::STextStyle& style, uint64_t format, SFieldProblem& problem)
{
	JsonValue const* const pText{ FindObject(root, {}, "text", true, problem) };

	if (pText != nullptr)
	{
		CheckFields(*pText, "text", format, problem);
		ReadName(*pText, "text", "encoding", Encodings, style.encoding, problem);
		ReadColourCodes(*pText, style, format, problem);
	}
}

//////////////////////////////////////////////////////////////////////////
// A brace that starts neither placeholder is a typo, which would otherwise reach the game unfilled.
void CheckPlaceholders(std::vector<std::string> const& arguments, std::string_view path, bool needsPassword, SFieldProblem& problem)
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
void ReadPasswordRules(JsonValue const& join, Query::SPasswordRules& rules, uint64_t format, SFieldProblem& problem)
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

		CheckFields(*it, Path, format, problem);

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
void ReadJoin(JsonValue const& root, Query::SJoinCommand& join, uint64_t format, SFieldProblem& problem)
{
	JsonValue const* const pJoin{ FindObject(root, {}, "join", true, problem) };

	if (pJoin != nullptr)
	{
		CheckFields(*pJoin, "join", format, problem);
		ReadRequiredStrings(*pJoin, "join", "arguments", join.arguments, problem);
		ReadRequiredStrings(*pJoin, "join", "passwordArguments", join.passwordArguments, problem);
		CheckPlaceholders(join.arguments, "join.arguments", false, problem);
		CheckPlaceholders(join.passwordArguments, "join.passwordArguments", true, problem);
		ReadPasswordRules(*pJoin, join.password, format, problem);
	}
}

//////////////////////////////////////////////////////////////////////////
// Null, with the problem set, when the protocol is not one of the loaded scripts.
Query::SProtocolDefinition const* ReadProtocol(JsonValue const& object, std::string_view parent, std::span<Query::SProtocolDefinition const> protocols,
	Query::EProtocol& protocol, SFieldProblem& problem)
{
	Query::SProtocolDefinition const* pFound{ nullptr };
	std::string name{};

	ReadRequiredString(object, parent, "protocol", name, problem);

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

		Fail(problem, JoinPath(parent, "protocol"), std::format("'{}' is not one of {}", name, known));
	}

	return pFound;
}

//////////////////////////////////////////////////////////////////////////
// Checked against what the script declares, so a game cannot pass an option its protocol never reads, or leave out one
// it needs.
void ReadProtocolOptions(JsonValue const& object, std::string_view parent, Query::SProtocolDefinition const& protocol,
	std::map<std::string, std::string>& options, SFieldProblem& problem)
{
	std::string const optionsPath{ JoinPath(parent, "protocolOptions") };
	JsonValue const* const pOptions{ FindObject(object, parent, "protocolOptions", false, problem) };

	if (pOptions != nullptr)
	{
		for (auto const& item : pOptions->items())
		{
			std::string const path{ JoinPath(optionsPath, item.key()) };

			if (IsComment(item.key()))
			{
				CheckComment(*pOptions, optionsPath, item.key(), item.value(), problem);
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
			Fail(problem, JoinPath(optionsPath, option.name), std::format("is missing; the {} protocol requires it", protocol.name));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadPortOffset(JsonValue const& object, std::string_view parent, std::string_view key, int32_t& offset, SFieldProblem& problem)
{
	JsonValue::const_iterator const it{ object.find(key) };

	if (it != object.cend())
	{
		bool const isValid{ (it->is_number_unsigned() && it->get<uint64_t>() <= static_cast<uint64_t>(MaxPortOffset))
			|| (it->is_number_integer() && !it->is_number_unsigned() && it->get<int64_t>() >= -MaxPortOffset) };

		if (isValid)
		{
			offset = static_cast<int32_t>(it->get<int64_t>());
		}
		else
		{
			Fail(problem, JoinPath(parent, key), std::format("must be a whole number from {} to {}", -MaxPortOffset, MaxPortOffset));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadMasters(JsonValue const& root, Query::SGameDefinition& game, uint64_t format, SFieldProblem& problem)
{
	JsonValue::const_iterator const it{ root.find("masters") };

	if (it != root.cend())
	{
		ReadObjects(*it, "masters", format, problem, [&game, &problem](JsonValue const& object, std::string_view path)
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
void ReadGame(JsonValue const& root, std::span<Query::SProtocolDefinition const> protocols, uint64_t format, Query::SGameDefinition& game, SFieldProblem& problem)
{
	CheckFields(root, {}, format, problem);
	ReadRequiredString(root, {}, "name", game.name, problem);

	Query::SProtocolDefinition const* const pProtocol{ ReadProtocol(root, {}, protocols, game.protocol, problem) };

	if (pProtocol != nullptr)
	{
		ReadProtocolOptions(root, {}, *pProtocol, game.protocolOptions, problem);
	}

	ReadPortOffset(root, {}, "queryPortOffset", game.queryPortOffset, problem);
	ReadMasters(root, game, format, problem);
	ReadKeys(root, game.keys, format, problem);
	ReadModes(root, game.modes, format, problem);
	ReadForeignServers(root, game.foreignServers, format, problem);
	ReadText(root, game.text, format, problem);
	ReadJoin(root, game.join, format, problem);
	ReadLaunch(root, game.launch, format, problem);
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::span<SGameField const> GetGameFields()
{
	return Fields;
}

//////////////////////////////////////////////////////////////////////////
// Without the conversations, which only the protocol and its options can change, so an editor need not run them on
// every keystroke.
std::expected<Query::SGameDefinition, SFieldProblem> ReadGameFields(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	Query::SGameDefinition game{};
	SFieldProblem problem{};

	if (root.is_discarded())
	{
		problem.reason = std::format("it is not valid JSON: {}", Json::DescribeSyntaxError(text, IgnoreComments));
	}
	else if (!root.is_object())
	{
		problem.reason = "it must hold a JSON object";
	}
	else if (std::optional<uint64_t> const format{ ReadFormat(root, problem) }; format.has_value())
	{
		ReadGame(root, protocols, *format, game, problem);
	}

	std::expected<Query::SGameDefinition, SFieldProblem> result{ std::move(game) };

	if (!problem.reason.empty())
	{
		result = std::unexpected{ std::move(problem) };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<Query::SGameDefinition, std::string> ReadGameJson(std::string_view text, std::span<Query::SProtocolDefinition const> protocols)
{
	std::expected<Query::SGameDefinition, SFieldProblem> read{ ReadGameFields(text, protocols) };
	std::expected<Query::SGameDefinition, std::string> result{};

	if (read.has_value())
	{
		result = std::move(*read);
	}
	else
	{
		result = std::unexpected{ read.error().path.empty() ? std::move(read.error().reason) : std::format("{}: {}", read.error().path, read.error().reason) };
	}

	return result;
}
} // namespace Lkt::Games
