#include "launch_layout.hpp"
#include "games/game_fields.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace Lkt::Games
{
namespace
{
using JsonValue = nlohmann::ordered_json;

constexpr std::string_view LaunchKey{ "launch" };
constexpr std::string_view LinuxKey{ "linux" };
constexpr std::string_view WindowsKey{ "windows" };
constexpr std::string_view LinuxSchema{ "launch.linux" };
constexpr std::string_view DesktopFilesKey{ "desktopFiles" };
constexpr std::array<std::string_view, 3> InstallFields{ "installDir", "program", "requiredFiles" };
constexpr std::string_view CommentPrefix{ "//" };

//////////////////////////////////////////////////////////////////////////
bool IsLinuxField(std::string_view name)
{
	return std::ranges::any_of(GetGameFields(), [name](SGameField const& field) { return field.parent == LinuxSchema && field.name == name; });
}

//////////////////////////////////////////////////////////////////////////
// A Linux field, or the note on one, which travels with it.
bool IsLinuxKey(std::string_view key)
{
	return IsLinuxField(key.starts_with(CommentPrefix) ? key.substr(CommentPrefix.size()) : key);
}

//////////////////////////////////////////////////////////////////////////
JsonValue const* FindLaunch(JsonValue const& game)
{
	JsonValue const* pLaunch{ nullptr };

	if (game.is_object())
	{
		JsonValue::const_iterator const it{ game.find(LaunchKey) };

		if (it != game.cend() && it->is_object())
		{
			pLaunch = &*it;
		}
	}

	return pLaunch;
}

//////////////////////////////////////////////////////////////////////////
JsonValue* FindLaunch(JsonValue& game)
{
	return const_cast<JsonValue*>(FindLaunch(std::as_const(game)));
}

//////////////////////////////////////////////////////////////////////////
bool HoldsTheInstallFolder(JsonValue const& facts)
{
	return std::ranges::all_of(InstallFields, [&facts](std::string_view field) { return facts.contains(field); });
}
} // namespace

//////////////////////////////////////////////////////////////////////////
bool HasPlatformGroups(JsonValue const& game)
{
	JsonValue const* const pLaunch{ FindLaunch(game) };

	return pLaunch != nullptr && (pLaunch->contains(LinuxKey) || pLaunch->contains(WindowsKey));
}

//////////////////////////////////////////////////////////////////////////
// Format 1 holds launch facts only for Linux and only with an install folder, whose desktop files may be an empty list,
// so anything else needs format 3's groups.
bool NeedsPlatformGroups(JsonValue const& game)
{
	JsonValue const* const pLaunch{ FindLaunch(game) };
	bool needs{ false };

	if (pLaunch != nullptr)
	{
		JsonValue::const_iterator const facts{ pLaunch->find(LinuxKey) };
		bool const holdsOnlyLinux{ pLaunch->size() == 1 && facts != pLaunch->cend() && facts->is_object() };

		needs = !holdsOnlyLinux || !HoldsTheInstallFolder(*facts);
	}

	return needs;
}

//////////////////////////////////////////////////////////////////////////
void NestLinuxFacts(JsonValue& game)
{
	JsonValue* const pLaunch{ FindLaunch(game) };

	if (pLaunch != nullptr && !pLaunch->contains(LinuxKey))
	{
		JsonValue facts = JsonValue::object();
		JsonValue rest = JsonValue::object();

		for (auto const& item : pLaunch->items())
		{
			(IsLinuxKey(item.key()) ? facts : rest)[item.key()] = item.value();
		}

		if (!facts.empty())
		{
			rest[std::string{ LinuxKey }] = std::move(facts);
			*pLaunch = std::move(rest);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Format 1 needs all four facts, so desktop files left out are written as the empty list it reads the same way.
void FlattenLinuxFacts(JsonValue& game)
{
	JsonValue* const pLaunch{ FindLaunch(game) };

	if (pLaunch != nullptr && pLaunch->size() == 1)
	{
		JsonValue::iterator const facts{ pLaunch->find(LinuxKey) };

		if (facts != pLaunch->end() && facts->is_object())
		{
			JsonValue flat = JsonValue::object();

			if (!facts->contains(DesktopFilesKey))
			{
				flat[std::string{ DesktopFilesKey }] = JsonValue::array();
			}

			for (auto const& item : facts->items())
			{
				flat[item.key()] = item.value();
			}

			*pLaunch = std::move(flat);
		}
	}
}
} // namespace Lkt::Games
