#include "script_calls.hpp"
#include "load_call.hpp"
#include "parse_call.hpp"
#include "request_call.hpp"
#include "sandbox.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace Lkt::Script
{
namespace
{
constexpr lua_Integer Api{ 1 };
constexpr std::array<std::string_view, 6> ModuleFields{ "api", "options", "masterRequest", "statusRequest", "parseMasterReply",
	"parseStatusReply" };
constexpr std::array<std::string_view, 2> OptionFields{ "description", "required" };

constexpr std::array<std::pair<std::string_view, Query::EParseError>, 3> Reasons{ {
	{ "wrongHeader", Query::EParseError::WrongHeader },
	{ "truncated", Query::EParseError::Truncated },
	{ "malformed", Query::EParseError::Malformed }
} };

//////////////////////////////////////////////////////////////////////////
template<typename TCall>
TCall& GetCall(lua_State* pState)
{
	return *static_cast<TCall*>(lua_touserdata(pState, 1));
}

//////////////////////////////////////////////////////////////////////////
// Only the first problem is kept: later ones are usually its consequences.
void SetProblem(std::string& problem, std::string text)
{
	if (problem.empty())
	{
		problem = std::move(text);
	}
}

//////////////////////////////////////////////////////////////////////////
// For a value of type LUA_TSTRING only: on a number, lua_tolstring would convert it in place.
std::string_view ToStringView(lua_State* pState, int index)
{
	size_t length{ 0 };
	char const* const pText{ lua_tolstring(pState, index, &length) };

	return std::string_view{ pText, length };
}

//////////////////////////////////////////////////////////////////////////
// Raw, so a metatable the script set never runs; leaves the value on the stack and returns its type.
int PushField(lua_State* pState, int table, char const* pField)
{
	lua_pushstring(pState, pField);

	return lua_rawget(pState, table);
}

//////////////////////////////////////////////////////////////////////////
bool ReadInteger(lua_State* pState, int table, char const* pField, lua_Integer min, lua_Integer max, lua_Integer& value)
{
	bool const isInteger{ PushField(pState, table, pField) == LUA_TNUMBER && lua_isinteger(pState, -1) != 0 };
	lua_Integer const number{ isInteger ? lua_tointeger(pState, -1) : 0 };
	bool const isValid{ isInteger && number >= min && number <= max };

	if (isValid)
	{
		value = number;
	}

	lua_pop(pState, 1);

	return isValid;
}

//////////////////////////////////////////////////////////////////////////
// True when the field is absent, or holds an integer in range.
bool ReadOptionalInteger(lua_State* pState, int table, char const* pField, lua_Integer min, lua_Integer max, std::optional<lua_Integer>& value)
{
	int const type{ PushField(pState, table, pField) };
	bool const isInteger{ type == LUA_TNUMBER && lua_isinteger(pState, -1) != 0 };
	lua_Integer const number{ isInteger ? lua_tointeger(pState, -1) : 0 };
	bool const isValid{ type == LUA_TNIL || (isInteger && number >= min && number <= max) };

	if (isInteger && isValid)
	{
		value = number;
	}

	lua_pop(pState, 1);

	return isValid;
}

//////////////////////////////////////////////////////////////////////////
bool ReadString(lua_State* pState, int table, char const* pField, std::string& value)
{
	bool const isString{ PushField(pState, table, pField) == LUA_TSTRING };

	if (isString)
	{
		value.assign(ToStringView(pState, -1));
	}

	lua_pop(pState, 1);

	return isString;
}

//////////////////////////////////////////////////////////////////////////
// Nil, or one of the reasons a reply can be wrong.
bool ReadReason(lua_State* pState, int index, std::optional<Query::EParseError>& reason)
{
	bool isValid{ lua_isnil(pState, index) };

	if (lua_type(pState, index) == LUA_TSTRING)
	{
		std::string_view const text{ ToStringView(pState, index) };
		auto const it{ std::ranges::find(Reasons, text, &std::pair<std::string_view, Query::EParseError>::first) };

		isValid = it != Reasons.end();
		reason = isValid ? std::optional<Query::EParseError>{ it->second } : std::nullopt;
	}

	return isValid;
}

//////////////////////////////////////////////////////////////////////////
// The smallest unknown name, so the problem names the same field whatever order the hash table iterates in.
void FindUnknownField(lua_State* pState, int table, std::span<std::string_view const> fields, std::string& firstUnknown, bool& hasUnnamedKey)
{
	lua_pushnil(pState);

	while (lua_next(pState, table) != 0)
	{
		if (lua_type(pState, -2) != LUA_TSTRING)
		{
			hasUnnamedKey = true;
		}
		else if (std::string_view const name{ ToStringView(pState, -2) }; !std::ranges::contains(fields, name)
			&& (firstUnknown.empty() || name < firstUnknown))
		{
			firstUnknown.assign(name);
		}

		lua_pop(pState, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
bool IsIdentifier(std::string_view name)
{
	auto const isWordCharacter{ [](char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); } };

	return !name.empty() && !(name.front() >= '0' && name.front() <= '9') && std::ranges::all_of(name, isWordCharacter);
}

//////////////////////////////////////////////////////////////////////////
void ReadFunction(lua_State* pState, int module, char const* pName, int& reference, std::string& problem)
{
	if (PushField(pState, module, pName) == LUA_TFUNCTION)
	{
		reference = luaL_ref(pState, LUA_REGISTRYINDEX);
	}
	else
	{
		lua_pop(pState, 1);
		SetProblem(problem, std::format("{} must be a function", pName));
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadOption(lua_State* pState, int options, Query::SProtocolOption& option, SLoadCall& call)
{
	lua_pushlstring(pState, option.name.data(), option.name.size());

	if (!IsIdentifier(option.name))
	{
		SetProblem(call.problem, std::format("options: '{}' is not a name a script can use", option.name));
	}
	else if (lua_rawget(pState, options) != LUA_TTABLE)
	{
		SetProblem(call.problem, std::format("options.{} must be a table", option.name));
	}
	else
	{
		int const entry{ lua_gettop(pState) };
		bool hasUnnamedKey{ false };

		call.unknownField.clear();
		FindUnknownField(pState, entry, OptionFields, call.unknownField, hasUnnamedKey);

		if (hasUnnamedKey)
		{
			SetProblem(call.problem, std::format("options.{} may hold only named fields", option.name));
		}
		else if (!call.unknownField.empty())
		{
			SetProblem(call.problem, std::format("options.{}.{} is not a field of an option", option.name, call.unknownField));
		}

		if (PushField(pState, entry, "required") == LUA_TBOOLEAN)
		{
			option.isRequired = lua_toboolean(pState, -1) != 0;
		}
		else
		{
			SetProblem(call.problem, std::format("options.{}.required must be true or false", option.name));
		}

		lua_pop(pState, 1);

		if (!ReadString(pState, entry, "description", option.description) || option.description.empty() || option.description.contains('\0'))
		{
			SetProblem(call.problem, std::format("options.{}.description must be a non-empty string", option.name));
		}
	}

	lua_pop(pState, 1);
}

//////////////////////////////////////////////////////////////////////////
void ReadOptions(lua_State* pState, int module, SLoadCall& call)
{
	int const type{ PushField(pState, module, "options") };
	int const options{ lua_gettop(pState) };

	if (type == LUA_TTABLE)
	{
		lua_pushnil(pState);

		while (lua_next(pState, options) != 0)
		{
			if (lua_type(pState, -2) == LUA_TSTRING)
			{
				call.options.emplace_back().name.assign(ToStringView(pState, -2));
			}
			else
			{
				SetProblem(call.problem, "options may hold only named entries");
			}

			lua_pop(pState, 1);
		}

		// Sorted, so options keep one order and problems name the same option whatever order the hash table iterates in.
		std::ranges::sort(call.options, {}, &Query::SProtocolOption::name);

		for (Query::SProtocolOption& option : call.options)
		{
			ReadOption(pState, options, option, call);
		}
	}
	else if (type != LUA_TNIL)
	{
		SetProblem(call.problem, "options must be a table");
	}

	lua_pop(pState, 1);
}

//////////////////////////////////////////////////////////////////////////
void ReadModule(lua_State* pState, int module, SLoadCall& call)
{
	lua_Integer api{ 0 };
	bool hasUnnamedKey{ false };

	if (lua_type(pState, module) != LUA_TTABLE)
	{
		SetProblem(call.problem, "the script must return a table");
	}
	else
	{
		FindUnknownField(pState, module, ModuleFields, call.unknownField, hasUnnamedKey);

		if (hasUnnamedKey)
		{
			SetProblem(call.problem, "the returned table may hold only named fields");
		}
		else if (!call.unknownField.empty())
		{
			SetProblem(call.problem, std::format("{} is not a field of script API {}", call.unknownField, Api));
		}

		if (!ReadInteger(pState, module, "api", Api, Api, api))
		{
			SetProblem(call.problem, std::format("api must be {}, the script API this Lookout runs", Api));
		}

		ReadFunction(pState, module, "masterRequest", call.masterRequest, call.problem);
		ReadFunction(pState, module, "statusRequest", call.statusRequest, call.problem);
		ReadFunction(pState, module, "parseMasterReply", call.parseMasterReply, call.problem);
		ReadFunction(pState, module, "parseStatusReply", call.parseStatusReply, call.problem);
		ReadOptions(pState, module, call);
	}
}

//////////////////////////////////////////////////////////////////////////
void PushDatagram(lua_State* pState, std::span<std::byte const> datagram)
{
	char const* const pBytes{ datagram.empty() ? "" : reinterpret_cast<char const*>(datagram.data()) };

	lua_pushlstring(pState, pBytes, datagram.size());
}

//////////////////////////////////////////////////////////////////////////
void ReadServers(lua_State* pState, int servers, SParseCall& call)
{
	constexpr lua_Integer MaxIp{ std::numeric_limits<uint32_t>::max() };
	constexpr lua_Integer MaxPort{ std::numeric_limits<uint16_t>::max() };
	bool isReading{ true };

	for (lua_Integer index{ 1 }; isReading; ++index)
	{
		int const type{ lua_rawgeti(pState, servers, index) };
		lua_Integer ip{ 0 };
		lua_Integer port{ 0 };

		if (type == LUA_TTABLE && ReadInteger(pState, lua_gettop(pState), "ip", 0, MaxIp, ip)
			&& ReadInteger(pState, lua_gettop(pState), "port", 0, MaxPort, port))
		{
			call.pServers->emplace_back(Query::SServerAddress{ static_cast<uint32_t>(ip), static_cast<uint16_t>(port) });
		}
		else if (type != LUA_TNIL)
		{
			SetProblem(call.problem, std::format("servers[{}] must hold an integer ip from 0 to {} and port from 0 to {}", index, MaxIp, MaxPort));
		}

		isReading = type != LUA_TNIL && call.problem.empty();
		lua_pop(pState, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadRules(lua_State* pState, int rules, SParseCall& call)
{
	bool isReading{ true };

	for (lua_Integer index{ 1 }; isReading; ++index)
	{
		int const type{ lua_rawgeti(pState, rules, index) };

		if (type == LUA_TTABLE)
		{
			Query::SRule& rule{ call.pReply->rules.emplace_back() };

			if (!ReadString(pState, lua_gettop(pState), "key", rule.key) || !ReadString(pState, lua_gettop(pState), "value", rule.value))
			{
				SetProblem(call.problem, std::format("rules[{}] must hold a string key and value", index));
			}
		}
		else if (type != LUA_TNIL)
		{
			SetProblem(call.problem, std::format("rules[{}] must be a table", index));
		}

		isReading = type != LUA_TNIL && call.problem.empty();
		lua_pop(pState, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
// An ordered array, so fields keep the protocol's order rather than a hash table's.
bool ReadFields(lua_State* pState, int player, std::vector<Query::SRule>& fields)
{
	int const type{ PushField(pState, player, "fields") };
	int const table{ lua_gettop(pState) };
	bool isValid{ type == LUA_TNIL || type == LUA_TTABLE };

	for (lua_Integer index{ 1 }; isValid && type == LUA_TTABLE && lua_rawgeti(pState, table, index) != LUA_TNIL; ++index)
	{
		int const entry{ lua_gettop(pState) };
		Query::SRule& field{ fields.emplace_back() };

		isValid = lua_type(pState, entry) == LUA_TTABLE && ReadString(pState, entry, "key", field.key) && ReadString(pState, entry, "value", field.value);
		lua_pop(pState, 1);
	}

	lua_settop(pState, table - 1);

	return isValid;
}

//////////////////////////////////////////////////////////////////////////
void ReadPlayers(lua_State* pState, int players, SParseCall& call)
{
	constexpr lua_Integer MinScore{ std::numeric_limits<int32_t>::min() };
	constexpr lua_Integer MaxScore{ std::numeric_limits<int32_t>::max() };
	constexpr lua_Integer MaxPing{ std::numeric_limits<uint32_t>::max() };
	bool isReading{ true };

	for (lua_Integer index{ 1 }; isReading; ++index)
	{
		int const type{ lua_rawgeti(pState, players, index) };

		if (type == LUA_TTABLE)
		{
			int const entry{ lua_gettop(pState) };
			Query::SPlayer& player{ call.pReply->players.emplace_back() };
			std::optional<lua_Integer> score{};
			std::optional<lua_Integer> ping{};

			if (ReadString(pState, entry, "name", player.name) && ReadOptionalInteger(pState, entry, "score", MinScore, MaxScore, score)
				&& ReadOptionalInteger(pState, entry, "ping", 0, MaxPing, ping) && ReadFields(pState, entry, player.fields))
			{
				player.score = score.has_value() ? std::optional<int32_t>{ static_cast<int32_t>(*score) } : std::nullopt;
				player.ping = ping.has_value() ? std::optional<uint32_t>{ static_cast<uint32_t>(*ping) } : std::nullopt;
			}
			else
			{
				SetProblem(call.problem, std::format("players[{}] must hold a string name, and may hold an int32 score, a uint32 ping and "
					"fields of string keys and values", index));
			}
		}
		else if (type != LUA_TNIL)
		{
			SetProblem(call.problem, std::format("players[{}] must be a table", index));
		}

		isReading = type != LUA_TNIL && call.problem.empty();
		lua_pop(pState, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadStatusReply(lua_State* pState, int reply, SParseCall& call)
{
	constexpr lua_Integer MaxCount{ std::numeric_limits<uint32_t>::max() };
	std::optional<lua_Integer> numMalformed{};

	if (PushField(pState, reply, "rules") == LUA_TTABLE)
	{
		ReadRules(pState, lua_gettop(pState), call);
	}
	else
	{
		SetProblem(call.problem, "rules must be a table");
	}

	lua_pop(pState, 1);

	if (PushField(pState, reply, "players") == LUA_TTABLE)
	{
		ReadPlayers(pState, lua_gettop(pState), call);
	}
	else
	{
		SetProblem(call.problem, "players must be a table");
	}

	lua_pop(pState, 1);

	if (ReadOptionalInteger(pState, reply, "malformedPlayerLines", 0, MaxCount, numMalformed))
	{
		call.pReply->numMalformedPlayerLines = static_cast<uint32_t>(numMalformed.value_or(0));
	}
	else
	{
		SetProblem(call.problem, std::format("malformedPlayerLines must be an integer from 0 to {}", MaxCount));
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
int LoadScript(lua_State* pState)
{
	constexpr int Environment{ 2 };
	constexpr int Module{ 3 };
	SLoadCall& call{ GetCall<SLoadCall>(pState) };

	lua_pushcfunction(pState, OpenSandbox);
	lua_call(pState, 0, 1);

	if (luaL_loadbufferx(pState, call.source.data(), call.source.size(), call.chunkName.c_str(), "t") != LUA_OK)
	{
		lua_error(pState);
	}

	lua_pushvalue(pState, Environment);
	lua_setupvalue(pState, -2, 1);
	lua_call(pState, 0, 1);
	ReadModule(pState, Module, call);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CallRequest(lua_State* pState)
{
	SRequestCall& call{ GetCall<SRequestCall>(pState) };

	lua_rawgeti(pState, LUA_REGISTRYINDEX, call.function);
	lua_createtable(pState, 0, static_cast<int>(call.pOptions->size()));

	for (auto const& [name, value] : *call.pOptions)
	{
		lua_pushlstring(pState, name.data(), name.size());
		lua_pushlstring(pState, value.data(), value.size());
		lua_rawset(pState, -3);
	}

	lua_call(pState, 1, 1);

	if (lua_type(pState, -1) == LUA_TSTRING)
	{
		std::string_view const bytes{ ToStringView(pState, -1) };

		call.bytes.resize(bytes.size());
		std::ranges::transform(bytes, call.bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
	}
	else
	{
		SetProblem(call.problem, "the request must be a string");
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CallParseMasterReply(lua_State* pState)
{
	constexpr int Servers{ 2 };
	constexpr int Reason{ 3 };
	SParseCall& call{ GetCall<SParseCall>(pState) };

	lua_rawgeti(pState, LUA_REGISTRYINDEX, call.function);
	PushDatagram(pState, call.datagram);
	lua_call(pState, 1, 2);

	if (lua_type(pState, Servers) == LUA_TTABLE)
	{
		ReadServers(pState, Servers, call);
	}
	else
	{
		SetProblem(call.problem, "the first result must be a table of servers");
	}

	if (!ReadReason(pState, Reason, call.reason))
	{
		SetProblem(call.problem, "the reason must be nil, \"wrongHeader\", \"truncated\" or \"malformed\"");
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CallParseStatusReply(lua_State* pState)
{
	constexpr int Reply{ 2 };
	constexpr int Reason{ 3 };
	SParseCall& call{ GetCall<SParseCall>(pState) };

	lua_rawgeti(pState, LUA_REGISTRYINDEX, call.function);
	PushDatagram(pState, call.datagram);
	lua_call(pState, 1, 2);

	if (lua_type(pState, Reply) == LUA_TTABLE && lua_isnil(pState, Reason))
	{
		ReadStatusReply(pState, Reply, call);
	}
	else if (!lua_isnil(pState, Reply) || lua_isnil(pState, Reason))
	{
		SetProblem(call.problem, "the result must be a reply table, or nil and a reason");
	}
	else if (!ReadReason(pState, Reason, call.reason))
	{
		SetProblem(call.problem, "the reason must be \"wrongHeader\", \"truncated\" or \"malformed\"");
	}

	return 0;
}
} // namespace Lkt::Script
