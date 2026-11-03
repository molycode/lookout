#include "store_json.hpp"
#include "json/json.hpp"
#include "query/store_definition.hpp"
#include <gtest/gtest.h>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr std::string_view SteamStore{ R"json({
	"format": 1,
	"name": "Steam",
	"//arguments": "Steam passes what follows the app id on to the game.",
	"arguments": [ "-applaunch", "{id}" ],
	"libraries": "steam",
	"windows": [ { "folder": { "root": "HKCU", "key": "Software\\Valve\\Steam", "value": "SteamPath" }, "program": "steam.exe" } ],
	"linux": [ { "folder": "{dataHome}/Steam", "programs": [ "steam" ] },
		{ "folder": "{home}/snap/steam/common/.local/share/Steam", "programs": [ "/snap/bin/steam" ] } ]
})json" };

//////////////////////////////////////////////////////////////////////////
JsonValue MakeStore()
{
	return JsonValue::parse(SteamStore);
}

//////////////////////////////////////////////////////////////////////////
// Empty when the store reads.
std::string ReadProblem(JsonValue const& store)
{
	std::expected<Query::SStoreDefinition, std::string> const result{ ReadStoreJson(store.dump()) };

	return result.has_value() ? std::string{} : result.error();
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, ReadsSteam)
{
	std::expected<Query::SStoreDefinition, std::string> const store{ ReadStoreJson(SteamStore) };

	ASSERT_TRUE(store.has_value()) << store.error();
	EXPECT_EQ(store->name, "Steam");
	EXPECT_EQ(store->arguments, (std::vector<std::string>{ "-applaunch", "{id}" }));
	EXPECT_EQ(store->libraries, Query::EStoreLibraries::Steam);
	EXPECT_EQ(store->onWindows, (std::vector<Query::SWindowsStoreInstall>{ Query::SWindowsStoreInstall{
		Query::SStoreRegistryValue{ Query::EStoreRegistryRoot::CurrentUser, "Software\\Valve\\Steam", "SteamPath" }, "steam.exe" } }));
	EXPECT_EQ(store->onLinux, (std::vector<Query::SLinuxStoreInstall>{ Query::SLinuxStoreInstall{ "{dataHome}/Steam", { "steam" } },
		Query::SLinuxStoreInstall{ "{home}/snap/steam/common/.local/share/Steam", { "/snap/bin/steam" } } }));
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, UnknownFieldIsRejected)
{
	JsonValue store = MakeStore();

	store["linux"][0]["program"] = "steam";

	EXPECT_EQ(ReadProblem(store), "linux[0].program: is not a field of format 1");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, NewerFormatAsksForANewerLookout)
{
	JsonValue store = MakeStore();

	store["format"] = 2;

	EXPECT_EQ(ReadProblem(store), "format: is 2, which needs a newer Lookout");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, ArgumentsMustHoldTheId)
{
	JsonValue store = MakeStore();

	store["arguments"] = JsonValue::parse(R"json([ "-applaunch" ])json");

	EXPECT_EQ(ReadProblem(store), "arguments: must hold {id}, the game's id in the store");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, OtherPlaceholderIsRejected)
{
	JsonValue store = MakeStore();

	store["arguments"] = JsonValue::parse(R"json([ "-applaunch", "{id}", "{address}" ])json");

	EXPECT_EQ(ReadProblem(store), "arguments[2]: holds a placeholder other than {id}");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, UnknownLibrariesAreRejected)
{
	JsonValue store = MakeStore();

	store["libraries"] = "gog";

	EXPECT_EQ(ReadProblem(store), "libraries: 'gog' is not one of steam");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, RegistryKeyMustBeAscii)
{
	JsonValue store = MakeStore();

	store["windows"][0]["folder"]["key"] = "Software\\Välve";

	EXPECT_EQ(ReadProblem(store), "windows[0].folder.key: must be printable ASCII");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, WindowsProgramMustBeAFileName)
{
	JsonValue store = MakeStore();

	store["windows"][0]["program"] = "bin/steam.exe";

	EXPECT_EQ(ReadProblem(store), "windows[0].program: must be a file name inside the folder");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, LinuxFolderMustStartWithAPlaceholderOrTheRoot)
{
	JsonValue store = MakeStore();

	store["linux"][0]["folder"] = "Steam";

	EXPECT_EQ(ReadProblem(store), "linux[0].folder: must start with {home}/, {dataHome}/ or /, with no . or .. between folders");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, LinuxFolderCannotStepOutOfItsStart)
{
	JsonValue store = MakeStore();

	store["linux"][0]["folder"] = "{home}/../Steam";

	EXPECT_EQ(ReadProblem(store), "linux[0].folder: must start with {home}/, {dataHome}/ or /, with no . or .. between folders");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, LinuxProgramIsANameOrAPath)
{
	JsonValue store = MakeStore();

	store["linux"][0]["programs"] = JsonValue::parse(R"json([ "bin/steam" ])json");

	EXPECT_EQ(ReadProblem(store), "linux[0].programs[0]: must be a name looked up on PATH, or a path like a folder's");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, LinuxInstallNeedsAProgram)
{
	JsonValue store = MakeStore();

	store["linux"][0]["programs"] = JsonValue::array();

	EXPECT_EQ(ReadProblem(store), "linux[0].programs: must name at least one program");
}

//////////////////////////////////////////////////////////////////////////
TEST(StoreJson, StoreWithoutAPlatformIsRejected)
{
	JsonValue store = MakeStore();

	store.erase("windows");
	store.erase("linux");

	EXPECT_EQ(ReadProblem(store), "must hold windows, linux or both");
}
} // namespace
} // namespace Lkt::Games
