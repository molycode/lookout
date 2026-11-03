#include "launch_layout.hpp"
#include "format_three_ids.hpp"
#include "games/game_fields.hpp"
#include "games/game_format.hpp"
#include <algorithm>
#include <array>
#include <format>
#include <optional>
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
constexpr std::string_view StoresKey{ "stores" };

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

//////////////////////////////////////////////////////////////////////////
std::string MakeComment(std::string_view key)
{
	return std::format("{}{}", CommentPrefix, key);
}

//////////////////////////////////////////////////////////////////////////
// A number is written as its digits; anything else is moved as it is, for the reader to name.
JsonValue ToStoreId(JsonValue const& value)
{
	JsonValue id = value;

	if (value.is_number_unsigned())
	{
		id = std::format("{}", value.get<uint64_t>());
	}

	return id;
}

//////////////////////////////////////////////////////////////////////////
std::optional<uint64_t> ToFormatThreeId(JsonValue const& value, uint64_t max)
{
	std::optional<uint64_t> const number{ value.is_string() ? ReadCanonicalDecimal(value.get_ref<std::string const&>()) : std::nullopt };

	return (number.has_value() && *number <= max) ? number : std::nullopt;
}

//////////////////////////////////////////////////////////////////////////
// Format 3 holds Steam's and GOG's ids as numbers among the Windows facts, so only where there are some.
bool FitsFormatThree(JsonValue const& launch)
{
	JsonValue::const_iterator const stores{ launch.find(StoresKey) };
	JsonValue::const_iterator const windows{ launch.find(WindowsKey) };
	bool fits{ !launch.contains(MakeComment(StoresKey)) };

	if (stores != launch.cend())
	{
		fits = fits && stores->is_object() && windows != launch.cend() && windows->is_object();

		for (auto const& item : stores->items())
		{
			std::string_view const key{ item.key() };
			std::string_view const store{ key.starts_with(CommentPrefix) ? key.substr(CommentPrefix.size()) : key };
			SFormatThreeId const* const pId{ FindFormatThreeId(store) };

			fits = fits && pId != nullptr && (key.starts_with(CommentPrefix) || ToFormatThreeId(item.value(), pId->max).has_value());
		}
	}

	return fits;
}

//////////////////////////////////////////////////////////////////////////
// Back at the front of the Windows facts, Steam's then GOG's, where lookout-games keeps them.
void LowerStoreIds(JsonValue& launch)
{
	JsonValue::iterator const stores{ launch.find(StoresKey) };

	if (stores != launch.end())
	{
		JsonValue windows = JsonValue::object();

		for (SFormatThreeId const& id : GetFormatThreeIds())
		{
			JsonValue::const_iterator const comment{ stores->find(MakeComment(id.store)) };
			JsonValue::const_iterator const value{ stores->find(id.store) };

			if (comment != stores->cend())
			{
				windows[MakeComment(id.field)] = *comment;
			}

			if (value != stores->cend())
			{
				windows[std::string{ id.field }] = ToFormatThreeId(*value, id.max).value_or(0);
			}
		}

		for (auto const& item : launch[std::string{ WindowsKey }].items())
		{
			windows[item.key()] = item.value();
		}

		launch[std::string{ WindowsKey }] = std::move(windows);
		launch.erase(StoresKey);
	}
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

//////////////////////////////////////////////////////////////////////////
bool HasStoreIds(JsonValue const& game)
{
	JsonValue const* const pLaunch{ FindLaunch(game) };

	return pLaunch != nullptr && pLaunch->contains(StoresKey);
}

//////////////////////////////////////////////////////////////////////////
// Format 3's Steam and GOG ids, and their notes, become store ids ahead of the platforms, as format 4 holds them.
void LiftStoreIds(JsonValue& game)
{
	JsonValue* const pLaunch{ FindLaunch(game) };
	JsonValue* const pWindows{ (pLaunch != nullptr && pLaunch->contains(WindowsKey)) ? &(*pLaunch)[std::string{ WindowsKey }] : nullptr };

	if (pWindows != nullptr && pWindows->is_object()
		&& std::ranges::any_of(GetFormatThreeIds(), [pWindows](SFormatThreeId const& id) { return pWindows->contains(id.field); }))
	{
		JsonValue stores = JsonValue::object();
		JsonValue launch = JsonValue::object();

		for (SFormatThreeId const& id : GetFormatThreeIds())
		{
			JsonValue::const_iterator const comment{ pWindows->find(MakeComment(id.field)) };
			JsonValue::const_iterator const value{ pWindows->find(id.field) };

			if (comment != pWindows->cend())
			{
				stores[MakeComment(id.store)] = *comment;
			}

			if (value != pWindows->cend())
			{
				stores[std::string{ id.store }] = ToStoreId(*value);
			}

			pWindows->erase(MakeComment(id.field));
			pWindows->erase(id.field);
		}

		launch[std::string{ StoresKey }] = std::move(stores);

		for (auto const& item : pLaunch->items())
		{
			launch[item.key()] = item.value();
		}

		*pLaunch = std::move(launch);
	}
}

//////////////////////////////////////////////////////////////////////////
// So a description older Lookouts read stays readable to them.
uint64_t FindLowestFormat(JsonValue const& game)
{
	JsonValue nested = game;
	uint64_t format{ GameFormat };

	NestLinuxFacts(nested);
	LiftStoreIds(nested);

	JsonValue const* const pLaunch{ FindLaunch(nested) };

	if (!NeedsPlatformGroups(nested))
	{
		format = FirstGameFormat;
	}
	else if (pLaunch == nullptr || FitsFormatThree(*pLaunch))
	{
		format = PlatformGroupsFormat;
	}

	return format;
}

//////////////////////////////////////////////////////////////////////////
// From the form's layout, nested and with store ids, into the layout of a format that holds the game.
void ShapeForFormat(JsonValue& game, uint64_t format)
{
	JsonValue* const pLaunch{ FindLaunch(game) };

	if (format == FirstGameFormat)
	{
		FlattenLinuxFacts(game);
	}
	else if (format == PlatformGroupsFormat && pLaunch != nullptr)
	{
		LowerStoreIds(*pLaunch);
	}

	game["format"] = format;
}
} // namespace Lkt::Games
