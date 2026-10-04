#include "script_calls.hpp"
#include "conversation_call.hpp"
#include "end_call.hpp"
#include "load_call.hpp"
#include "sandbox.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
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
constexpr std::array<std::string_view, 4> ModuleFields{ "api", "options", "master", "server" };
constexpr std::array<std::string_view, 3> MasterFields{ "transport", "start", "receive" };
constexpr std::array<std::string_view, 3> ServerFields{ "start", "receive", "finish" };
constexpr std::array<std::string_view, 2> OptionFields{ "description", "required" };

constexpr std::array<std::string_view, 1> StartFields{ "send" };
constexpr std::array<std::string_view, 5> MasterReceiveFields{ "send", "servers", "done", "quiet", "reason" };
constexpr std::array<std::string_view, 4> ServerReceiveFields{ "send", "reply", "quiet", "reason" };
constexpr std::array<std::string_view, 2> FinishFields{ "reply", "reason" };

constexpr size_t MaxSends{ 8 };
// The largest IPv4 UDP payload.
constexpr size_t MaxSendSize{ 65507 };
constexpr lua_Integer MaxQuietMs{ 10000 };

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
// path names the table in problems; empty for the module itself.
void CheckModuleFields(lua_State* pState, int table, std::span<std::string_view const> fields, std::string_view path, SLoadCall& call)
{
	bool hasUnnamedKey{ false };

	call.unknownField.clear();
	FindUnknownField(pState, table, fields, call.unknownField, hasUnnamedKey);

	if (hasUnnamedKey)
	{
		SetProblem(call.problem, std::format("{} may hold only named fields", path.empty() ? std::string_view{ "the returned table" } : path));
	}
	else if (!call.unknownField.empty())
	{
		SetProblem(call.problem, std::format("{}{}{} is not a field of script API {}", path, path.empty() ? "" : ".", call.unknownField, Api));
	}
}

//////////////////////////////////////////////////////////////////////////
bool IsIdentifier(std::string_view name)
{
	auto const isWordCharacter{ [](char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); } };

	return !name.empty() && !(name.front() >= '0' && name.front() <= '9') && std::ranges::all_of(name, isWordCharacter);
}

//////////////////////////////////////////////////////////////////////////
void ReadFunction(lua_State* pState, int table, char const* pField, std::string_view path, int& reference, std::string& problem)
{
	if (PushField(pState, table, pField) == LUA_TFUNCTION)
	{
		reference = luaL_ref(pState, LUA_REGISTRYINDEX);
	}
	else
	{
		lua_pop(pState, 1);
		SetProblem(problem, std::format("{} must be a function", path));
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
void ReadMaster(lua_State* pState, int module, SLoadCall& call)
{
	if (PushField(pState, module, "master") == LUA_TTABLE)
	{
		int const master{ lua_gettop(pState) };
		bool const isString{ PushField(pState, master, "transport") == LUA_TSTRING };
		bool const isUdp{ isString && ToStringView(pState, -1) == "udp" };
		bool const isTcp{ isString && ToStringView(pState, -1) == "tcp" };

		lua_pop(pState, 1);
		CheckModuleFields(pState, master, MasterFields, "master", call);
		call.masterTransport = isTcp ? EMasterTransport::Tcp : EMasterTransport::Udp;

		if (!isUdp && !isTcp)
		{
			SetProblem(call.problem, "master.transport must be \"udp\" or \"tcp\"");
		}

		ReadFunction(pState, master, "start", "master.start", call.masterStart, call.problem);
		ReadFunction(pState, master, "receive", "master.receive", call.masterReceive, call.problem);
	}
	else
	{
		SetProblem(call.problem, "master must be a table");
	}

	lua_pop(pState, 1);
}

//////////////////////////////////////////////////////////////////////////
void ReadServer(lua_State* pState, int module, SLoadCall& call)
{
	if (PushField(pState, module, "server") == LUA_TTABLE)
	{
		int const server{ lua_gettop(pState) };

		CheckModuleFields(pState, server, ServerFields, "server", call);
		ReadFunction(pState, server, "start", "server.start", call.serverStart, call.problem);
		ReadFunction(pState, server, "receive", "server.receive", call.serverReceive, call.problem);

		int const finishType{ PushField(pState, server, "finish") };

		if (finishType == LUA_TFUNCTION)
		{
			call.serverFinish = luaL_ref(pState, LUA_REGISTRYINDEX);
		}
		else
		{
			lua_pop(pState, 1);

			if (finishType != LUA_TNIL)
			{
				SetProblem(call.problem, "server.finish must be a function when present");
			}
		}
	}
	else
	{
		SetProblem(call.problem, "server must be a table");
	}

	lua_pop(pState, 1);
}

//////////////////////////////////////////////////////////////////////////
void ReadModule(lua_State* pState, int module, SLoadCall& call)
{
	lua_Integer api{ 0 };

	if (lua_type(pState, module) != LUA_TTABLE)
	{
		SetProblem(call.problem, "the script must return a table");
	}
	else
	{
		CheckModuleFields(pState, module, ModuleFields, {}, call);

		if (!ReadInteger(pState, module, "api", Api, Api, api))
		{
			SetProblem(call.problem, std::format("api must be {}, the script API this Lookout runs", Api));
		}

		ReadMaster(pState, module, call);
		ReadServer(pState, module, call);
		ReadOptions(pState, module, call);
	}
}

//////////////////////////////////////////////////////////////////////////
void PushBytes(lua_State* pState, std::span<std::byte const> bytes)
{
	char const* const pBytes{ bytes.empty() ? "" : reinterpret_cast<char const*>(bytes.data()) };

	lua_pushlstring(pState, pBytes, bytes.size());
}

//////////////////////////////////////////////////////////////////////////
void PushOptions(lua_State* pState, std::map<std::string, std::string> const& options)
{
	lua_createtable(pState, 0, static_cast<int>(options.size()));

	for (auto const& [name, value] : options)
	{
		lua_pushlstring(pState, name.data(), name.size());
		lua_pushlstring(pState, value.data(), value.size());
		lua_rawset(pState, -3);
	}
}

//////////////////////////////////////////////////////////////////////////
void SetSendProblem(std::string& problem)
{
	SetProblem(problem, std::format("send must hold 1 to {} strings of 1 to {} bytes", MaxSends, MaxSendSize));
}

//////////////////////////////////////////////////////////////////////////
void ReadSend(lua_State* pState, int send, SConversationCall& call)
{
	bool isReading{ true };

	for (lua_Integer index{ 1 }; isReading; ++index)
	{
		int const type{ lua_rawgeti(pState, send, index) };
		size_t const size{ (type == LUA_TSTRING) ? lua_rawlen(pState, -1) : 0 };

		if (size >= 1 && size <= MaxSendSize && call.action.send.size() < MaxSends)
		{
			std::string_view const bytes{ ToStringView(pState, -1) };
			std::vector<std::byte>& datagram{ call.action.send.emplace_back(bytes.size()) };

			std::ranges::transform(bytes, datagram.begin(), [](char c) { return static_cast<std::byte>(c); });
		}
		else if (type != LUA_TNIL)
		{
			SetSendProblem(call.problem);
		}

		isReading = type != LUA_TNIL && call.problem.empty();
		lua_pop(pState, 1);
	}

	if (call.action.send.empty())
	{
		SetSendProblem(call.problem);
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadServers(lua_State* pState, int servers, SConversationCall& call)
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
			call.action.servers.emplace_back(Query::SServerAddress{ static_cast<uint32_t>(ip), static_cast<uint16_t>(port) });
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
void ReadRules(lua_State* pState, int rules, Query::SStatusReply& reply, std::string& problem)
{
	bool isReading{ true };

	for (lua_Integer index{ 1 }; isReading; ++index)
	{
		int const type{ lua_rawgeti(pState, rules, index) };

		if (type == LUA_TTABLE)
		{
			Query::SRule& rule{ reply.rules.emplace_back() };

			if (!ReadString(pState, lua_gettop(pState), "key", rule.key) || !ReadString(pState, lua_gettop(pState), "value", rule.value))
			{
				SetProblem(problem, std::format("reply.rules[{}] must hold a string key and value", index));
			}
		}
		else if (type != LUA_TNIL)
		{
			SetProblem(problem, std::format("reply.rules[{}] must be a table", index));
		}

		isReading = type != LUA_TNIL && problem.empty();
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
void ReadPlayers(lua_State* pState, int players, Query::SStatusReply& reply, std::string& problem)
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
			Query::SPlayer& player{ reply.players.emplace_back() };
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
				SetProblem(problem, std::format("reply.players[{}] must hold a string name, and may hold an int32 score, a uint32 ping and "
					"fields of string keys and values", index));
			}
		}
		else if (type != LUA_TNIL)
		{
			SetProblem(problem, std::format("reply.players[{}] must be a table", index));
		}

		isReading = type != LUA_TNIL && problem.empty();
		lua_pop(pState, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadStatusReply(lua_State* pState, int table, Query::SStatusReply& reply, std::string& problem)
{
	constexpr lua_Integer MaxCount{ std::numeric_limits<uint32_t>::max() };
	constexpr lua_Integer MaxPort{ std::numeric_limits<uint16_t>::max() };
	std::optional<lua_Integer> numMalformed{};
	std::optional<lua_Integer> joinPort{};

	if (PushField(pState, table, "rules") == LUA_TTABLE)
	{
		ReadRules(pState, lua_gettop(pState), reply, problem);
	}
	else
	{
		SetProblem(problem, "reply.rules must be a table");
	}

	lua_pop(pState, 1);

	if (PushField(pState, table, "players") == LUA_TTABLE)
	{
		ReadPlayers(pState, lua_gettop(pState), reply, problem);
	}
	else
	{
		SetProblem(problem, "reply.players must be a table");
	}

	lua_pop(pState, 1);

	if (ReadOptionalInteger(pState, table, "malformedPlayerLines", 0, MaxCount, numMalformed))
	{
		reply.numMalformedPlayerLines = static_cast<uint32_t>(numMalformed.value_or(0));
	}
	else
	{
		SetProblem(problem, std::format("reply.malformedPlayerLines must be an integer from 0 to {}", MaxCount));
	}

	if (ReadOptionalInteger(pState, table, "joinPort", 1, MaxPort, joinPort))
	{
		reply.joinPort = joinPort.transform([](lua_Integer port) { return static_cast<uint16_t>(port); });
	}
	else
	{
		SetProblem(problem, std::format("reply.joinPort must be an integer from 1 to {}", MaxPort));
	}
}

//////////////////////////////////////////////////////////////////////////
std::span<std::string_view const> GetActionFields(EConversationKind kind, ECallback callback)
{
	std::span<std::string_view const> fields{ StartFields };

	if (callback == ECallback::Receive && kind == EConversationKind::Master)
	{
		fields = MasterReceiveFields;
	}
	else if (callback == ECallback::Receive)
	{
		fields = ServerReceiveFields;
	}
	else if (callback == ECallback::Finish)
	{
		fields = FinishFields;
	}

	return fields;
}

//////////////////////////////////////////////////////////////////////////
// Every field is read, allowed here or not: the unknown-field check has already set the first problem.
void ReadActionFields(lua_State* pState, int table, SConversationCall& call)
{
	std::optional<lua_Integer> quiet{};

	if (int const type{ PushField(pState, table, "send") }; type == LUA_TTABLE)
	{
		ReadSend(pState, lua_gettop(pState), call);
	}
	else if (type != LUA_TNIL)
	{
		SetSendProblem(call.problem);
	}

	lua_pop(pState, 1);

	if (int const type{ PushField(pState, table, "servers") }; type == LUA_TTABLE)
	{
		ReadServers(pState, lua_gettop(pState), call);
	}
	else if (type != LUA_TNIL)
	{
		SetProblem(call.problem, "servers must be a table");
	}

	lua_pop(pState, 1);

	if (int const type{ PushField(pState, table, "reply") }; type == LUA_TTABLE)
	{
		ReadStatusReply(pState, lua_gettop(pState), call.action.reply.emplace(), call.problem);
	}
	else if (type != LUA_TNIL)
	{
		SetProblem(call.problem, "reply must be a table");
	}

	lua_pop(pState, 1);

	if (int const type{ PushField(pState, table, "done") }; type == LUA_TBOOLEAN && lua_toboolean(pState, -1) != 0)
	{
		call.action.isDone = true;
	}
	else if (type != LUA_TNIL)
	{
		SetProblem(call.problem, "done must be true");
	}

	lua_pop(pState, 1);

	if (ReadOptionalInteger(pState, table, "quiet", 1, MaxQuietMs, quiet))
	{
		call.action.quiet = quiet.transform([](lua_Integer ms) { return std::chrono::milliseconds{ ms }; });
	}
	else
	{
		SetProblem(call.problem, std::format("quiet must be an integer from 1 to {} milliseconds", MaxQuietMs));
	}

	if (int const type{ PushField(pState, table, "reason") }; type != LUA_TNIL)
	{
		std::string_view const text{ (type == LUA_TSTRING) ? ToStringView(pState, -1) : std::string_view{} };
		auto const it{ std::ranges::find(Reasons, text, &std::pair<std::string_view, Query::EParseError>::first) };

		if (it != Reasons.end())
		{
			call.action.reason = it->second;
		}
		else
		{
			SetProblem(call.problem, "reason must be \"wrongHeader\", \"truncated\" or \"malformed\"");
		}
	}

	lua_pop(pState, 1);
}

//////////////////////////////////////////////////////////////////////////
// A server's reply or reason and a master's done end the conversation; a master's reason only counts a bad datagram.
void CheckActionCombination(SConversationCall& call)
{
	SScriptAction const& action{ call.action };
	bool const isSending{ !action.send.empty() };
	bool const isEnding{ (call.kind == EConversationKind::Server) ? (action.reply.has_value() || action.reason.has_value()) : action.isDone };

	if (isSending && action.quiet.has_value())
	{
		SetProblem(call.problem, "send and quiet exclude each other");
	}
	else if (action.reply.has_value() && action.reason.has_value())
	{
		SetProblem(call.problem, "reply and reason exclude each other");
	}
	else if (isEnding && (isSending || action.quiet.has_value()))
	{
		SetProblem(call.problem, "an action that ends the conversation excludes send and quiet");
	}
}

//////////////////////////////////////////////////////////////////////////
// Nil means nothing to do yet. A datagram conversation starts by sending; over a stream the master speaks first.
void ReadAction(lua_State* pState, int action, SConversationCall& call)
{
	if (lua_type(pState, action) == LUA_TTABLE)
	{
		bool hasUnnamedKey{ false };

		call.unknownField.clear();
		FindUnknownField(pState, action, GetActionFields(call.kind, call.callback), call.unknownField, hasUnnamedKey);

		if (hasUnnamedKey)
		{
			SetProblem(call.problem, "the action may hold only named fields");
		}
		else if (!call.unknownField.empty())
		{
			SetProblem(call.problem, std::format("the action may not hold {}", call.unknownField));
		}

		ReadActionFields(pState, action, call);
		CheckActionCombination(call);
	}
	else if (!lua_isnil(pState, action))
	{
		SetProblem(call.problem, "the result must be an action table or nil");
	}

	if (call.callback == ECallback::Start && !call.isStream && call.action.send.empty())
	{
		SetProblem(call.problem, "a UDP conversation must start by sending");
	}
	else if (call.callback == ECallback::Start && call.isStream && !call.action.send.empty())
	{
		SetProblem(call.problem, "a TCP master speaks first, so start may not send");
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
	lua_newtable(pState);
	call.states = luaL_ref(pState, LUA_REGISTRYINDEX);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// start(options, state), receive(state, data), finish(state); start first gives the conversation its state table.
int CallConversation(lua_State* pState)
{
	SConversationCall& call{ GetCall<SConversationCall>(pState) };
	lua_Integer const id{ static_cast<lua_Integer>(call.id) };

	lua_rawgeti(pState, LUA_REGISTRYINDEX, call.states);

	int const states{ lua_gettop(pState) };

	if (call.callback == ECallback::Start)
	{
		lua_newtable(pState);
		lua_rawseti(pState, states, id);
	}

	lua_rawgeti(pState, LUA_REGISTRYINDEX, call.function);

	if (call.callback == ECallback::Start)
	{
		PushOptions(pState, *call.pOptions);
	}

	if (lua_rawgeti(pState, states, id) != LUA_TTABLE)
	{
		luaL_error(pState, "the conversation has already ended");
	}

	if (call.callback == ECallback::Receive)
	{
		PushBytes(pState, call.data);
	}

	lua_call(pState, lua_gettop(pState) - states - 1, 1);
	ReadAction(pState, lua_gettop(pState), call);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Clears a slot without allocating, so it cannot fail; clearing it twice is harmless.
int EndConversation(lua_State* pState)
{
	SEndCall const& call{ GetCall<SEndCall>(pState) };

	lua_rawgeti(pState, LUA_REGISTRYINDEX, call.states);
	lua_pushnil(pState);
	lua_rawseti(pState, -2, static_cast<lua_Integer>(call.id));

	return 0;
}
} // namespace Lkt::Script
