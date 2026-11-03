#include "store_json.hpp"
#include "json_fields.hpp"
#include "games/field_problem.hpp"
#include "games/store_format.hpp"
#include "json/json.hpp"
#include "json/syntax_error.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
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
constexpr std::string_view IdPlaceholder{ "{id}" };
constexpr std::array<std::string_view, 6> StoreFields{ "format", "name", "arguments", "libraries", "windows", "linux" };
constexpr std::array<std::string_view, 2> WindowsFields{ "folder", "program" };
constexpr std::array<std::string_view, 3> RegistryFields{ "root", "key", "value" };
constexpr std::array<std::string_view, 2> LinuxFields{ "folder", "programs" };
constexpr std::array<std::string_view, 3> FolderStarts{ "{home}/", "{dataHome}/", "/" };

constexpr std::array<std::pair<std::string_view, Query::EStoreLibraries>, 1> Libraries{ {
	{ "steam", Query::EStoreLibraries::Steam }
} };

constexpr std::array<std::pair<std::string_view, Query::EStoreRegistryRoot>, 2> RegistryRoots{ {
	{ "HKCU", Query::EStoreRegistryRoot::CurrentUser },
	{ "HKLM", Query::EStoreRegistryRoot::LocalMachine }
} };

//////////////////////////////////////////////////////////////////////////
// A field this format does not know is a typo or a later format's, so it is never skipped silently.
void CheckFields(JsonValue const& object, std::string_view path, std::span<std::string_view const> fields, SFieldProblem& problem)
{
	for (auto const& item : object.items())
	{
		std::string_view const key{ item.key() };

		if (IsComment(key))
		{
			CheckComment(object, path, key, item.value(), problem);
		}
		else if (!std::ranges::contains(fields, key))
		{
			Fail(problem, JoinPath(path, key), std::format("is not a field of format {}", StoreFormat));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Read before anything else: a newer format's fields would otherwise only be reported as unknown.
bool ReadFormat(JsonValue const& root, SFieldProblem& problem)
{
	JsonValue::const_iterator const it{ root.find("format") };
	bool const isReadable{ it != root.cend() && it->is_number_unsigned() && it->get<uint64_t>() == StoreFormat };

	if (it == root.cend())
	{
		Fail(problem, "format", "is missing");
	}
	else if (it->is_number_unsigned() && it->get<uint64_t>() > StoreFormat)
	{
		Fail(problem, "format", std::format("is {}, which needs a newer Lookout", it->get<uint64_t>()));
	}
	else if (!isReadable)
	{
		Fail(problem, "format", std::format("must be {}", StoreFormat));
	}

	return isReadable;
}

//////////////////////////////////////////////////////////////////////////
// Each is one argument, so {id} cannot spill into another; at least one must carry it.
void ReadArguments(JsonValue const& root, std::vector<std::string>& arguments, SFieldProblem& problem)
{
	ReadRequiredStrings(root, {}, "arguments", arguments, problem);

	for (size_t index{ 0 }; index < arguments.size(); ++index)
	{
		std::string withoutId{ arguments[index] };

		for (size_t at{ withoutId.find(IdPlaceholder) }; at != std::string::npos; at = withoutId.find(IdPlaceholder))
		{
			withoutId.erase(at, IdPlaceholder.size());
		}

		if (withoutId.find_first_of("{}") != std::string::npos)
		{
			Fail(problem, std::format("arguments[{}]", index), "holds a placeholder other than {id}");
		}
	}

	if (!arguments.empty() && std::ranges::none_of(arguments, [](std::string const& argument) { return argument.contains(IdPlaceholder); }))
	{
		Fail(problem, "arguments", "must hold {id}, the game's id in the store");
	}
}

//////////////////////////////////////////////////////////////////////////
// Registry paths are ASCII, as Windows names its own keys and values, and Lookout widens them that way.
bool IsPrintableAscii(std::string_view text)
{
	return std::ranges::all_of(text, [](char c) { return c >= ' ' && c <= '~'; });
}

//////////////////////////////////////////////////////////////////////////
bool IsFileName(std::string_view name)
{
	return name != "." && name != ".." && name.find_first_of("/\\:{}") == std::string_view::npos;
}

//////////////////////////////////////////////////////////////////////////
// A placeholder or the root, then folders, never back up out of them.
bool IsStorePath(std::string_view path)
{
	auto const start{ std::ranges::find_if(FolderStarts, [path](std::string_view prefix) { return path.starts_with(prefix); }) };
	std::string_view rest{ (start != FolderStarts.end()) ? path.substr(start->size()) : std::string_view{} };
	bool isValid{ start != FolderStarts.end() && rest.find_first_of("{}\\") == std::string_view::npos };

	while (isValid && !rest.empty())
	{
		std::string_view const segment{ rest.substr(0, rest.find('/')) };

		isValid = segment != "." && segment != "..";
		rest.remove_prefix(std::min(rest.size(), segment.size() + 1));
	}

	return isValid;
}

//////////////////////////////////////////////////////////////////////////
void ReadRegistryValue(JsonValue const& entry, std::string_view parent, Query::SStoreRegistryValue& folder, SFieldProblem& problem)
{
	std::string const path{ JoinPath(parent, "folder") };
	JsonValue const* const pFolder{ FindObject(entry, parent, "folder", true, problem) };

	if (pFolder != nullptr)
	{
		CheckFields(*pFolder, path, RegistryFields, problem);
		ReadName(*pFolder, path, "root", RegistryRoots, folder.root, problem);
		ReadRequiredString(*pFolder, path, "key", folder.key, problem);
		ReadRequiredString(*pFolder, path, "value", folder.value, problem);

		if (!IsPrintableAscii(folder.key))
		{
			Fail(problem, JoinPath(path, "key"), "must be printable ASCII");
		}

		if (!IsPrintableAscii(folder.value))
		{
			Fail(problem, JoinPath(path, "value"), "must be printable ASCII");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename TRead>
void ReadInstalls(JsonValue const& root, std::string_view key, std::span<std::string_view const> fields, SFieldProblem& problem, TRead&& read)
{
	JsonValue::const_iterator const it{ root.find(key) };

	if (it != root.cend() && it->is_array())
	{
		size_t index{ 0 };

		for (JsonValue const& entry : *it)
		{
			std::string const path{ std::format("{}[{}]", key, index) };

			if (entry.is_object())
			{
				CheckFields(entry, path, fields, problem);
				read(entry, path);
			}
			else
			{
				Fail(problem, path, "must be an object");
			}

			++index;
		}
	}
	else if (it != root.cend())
	{
		Fail(problem, key, "must be an array");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadStore(JsonValue const& root, Query::SStoreDefinition& store, SFieldProblem& problem)
{
	CheckFields(root, {}, StoreFields, problem);
	ReadRequiredString(root, {}, "name", store.name, problem);
	ReadArguments(root, store.arguments, problem);
	ReadName(root, {}, "libraries", Libraries, store.libraries, problem);
	ReadInstalls(root, "windows", WindowsFields, problem, [&store, &problem](JsonValue const& entry, std::string_view path)
	{
		Query::SWindowsStoreInstall& install{ store.onWindows.emplace_back() };

		ReadRegistryValue(entry, path, install.folder, problem);
		ReadRequiredString(entry, path, "program", install.program, problem);

		if (!install.program.empty() && !IsFileName(install.program))
		{
			Fail(problem, JoinPath(path, "program"), "must be a file name inside the folder");
		}
	});
	ReadInstalls(root, "linux", LinuxFields, problem, [&store, &problem](JsonValue const& entry, std::string_view path)
	{
		Query::SLinuxStoreInstall& install{ store.onLinux.emplace_back() };

		ReadRequiredString(entry, path, "folder", install.folder, problem);
		ReadRequiredStrings(entry, path, "programs", install.programs, problem);

		if (!install.folder.empty() && !IsStorePath(install.folder))
		{
			Fail(problem, JoinPath(path, "folder"), "must start with {home}/, {dataHome}/ or /, with no . or .. between folders");
		}

		for (size_t index{ 0 }; index < install.programs.size(); ++index)
		{
			std::string_view const program{ install.programs[index] };

			if (!program.empty() && !IsFileName(program) && !IsStorePath(program))
			{
				Fail(problem, std::format("{}.programs[{}]", path, index), "must be a name looked up on PATH, or a path like a folder's");
			}
		}

		if (install.programs.empty())
		{
			Fail(problem, JoinPath(path, "programs"), "must name at least one program");
		}
	});

	if (!root.contains("windows") && !root.contains("linux"))
	{
		Fail(problem, {}, "must hold windows, linux or both");
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// The error names where the file first went wrong; the key is left for the caller, as it is the file's name.
std::expected<Query::SStoreDefinition, std::string> ReadStoreJson(std::string_view text)
{
	JsonValue const root = JsonValue::parse(text, nullptr, AllowExceptions, IgnoreComments);
	Query::SStoreDefinition store{};
	SFieldProblem problem{};

	if (root.is_discarded())
	{
		problem.reason = std::format("it is not valid JSON: {}", Json::DescribeSyntaxError(text, IgnoreComments));
	}
	else if (!root.is_object())
	{
		problem.reason = "it must hold a JSON object";
	}
	else if (ReadFormat(root, problem))
	{
		ReadStore(root, store, problem);
	}

	std::expected<Query::SStoreDefinition, std::string> result{ std::move(store) };

	if (!problem.reason.empty())
	{
		result = std::unexpected{ problem.path.empty() ? std::move(problem.reason) : std::format("{}: {}", problem.path, problem.reason) };
	}

	return result;
}
} // namespace Lkt::Games
