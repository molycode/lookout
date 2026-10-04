#include "script/protocol_script.hpp"
#include "callback.hpp"
#include "conversation_call.hpp"
#include "end_call.hpp"
#include "load_call.hpp"
#include "script_calls.hpp"
#include <tge/assert.hpp>
#include <array>
#include <cstdlib>
#include <format>
#include <string_view>
#include <utility>

namespace Lkt::Script
{
namespace
{
constexpr size_t MemoryLimit{ 16u << 20 };
// Large copies run in C between instruction checks, so a large allocation checks the deadline as well.
constexpr size_t LargeAllocation{ 64u << 10 };
constexpr std::chrono::milliseconds CallTimeLimit{ 10 };
constexpr int InstructionsPerCheck{ 1000 };

constexpr std::array<std::string_view, 2> KindNames{ "master", "server" };
constexpr std::array<std::string_view, 3> CallbackNames{ "start", "receive", "finish" };

//////////////////////////////////////////////////////////////////////////
int Panic(lua_State*)
{
	TGE_FATAL("Lua raised an error outside a protected call");

	return 0;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> CProtocolScript::Initialize(std::string_view name, std::string_view source)
{
	std::expected<void, std::string> result{};

	m_numBytes = 0;
	m_deadline = std::chrono::steady_clock::now() + CallTimeLimit;
	m_pState = lua_newstate(&CProtocolScript::Allocate, this, luaL_makeseed(nullptr));

	if (m_pState != nullptr)
	{
		SLoadCall call{};

		call.source = source;
		call.chunkName = std::format("={}", name);
		lua_atpanic(m_pState, &Panic);
		lua_sethook(m_pState, &CProtocolScript::CheckDeadline, LUA_MASKCOUNT, InstructionsPerCheck);

		std::string failure{ Run(&LoadScript, &call) };

		if (failure.empty())
		{
			failure = std::move(call.problem);
		}

		if (failure.empty())
		{
			m_masterStart = call.masterStart;
			m_masterReceive = call.masterReceive;
			m_serverStart = call.serverStart;
			m_serverReceive = call.serverReceive;
			m_serverFinish = call.serverFinish;
			m_states = call.states;
			m_masterTransport = call.masterTransport;
			m_options = std::move(call.options);
		}
		else
		{
			result = std::unexpected{ std::move(failure) };
		}
	}
	else
	{
		result = std::unexpected{ std::string{ "cannot create a Lua state" } };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CProtocolScript::Terminate()
{
	if (m_pState != nullptr)
	{
		lua_close(m_pState);
		m_pState = nullptr;
	}

	m_options.clear();
}

//////////////////////////////////////////////////////////////////////////
std::span<Query::SProtocolOption const> CProtocolScript::GetOptions() const
{
	return m_options;
}

//////////////////////////////////////////////////////////////////////////
EMasterTransport CProtocolScript::GetMasterTransport() const
{
	return m_masterTransport;
}

//////////////////////////////////////////////////////////////////////////
std::expected<SScriptAction, std::string> CProtocolScript::Start(SConversation& conversation, std::map<std::string, std::string> const& options)
{
	TGE_ASSERT(conversation.id == 0, "The conversation has already started");

	conversation.id = ++m_lastConversationId;

	std::expected<SScriptAction, std::string> result{ Call(conversation, ECallback::Start, {}, &options) };

	if (!result.has_value())
	{
		End(conversation);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<SScriptAction, std::string> CProtocolScript::Receive(SConversation const& conversation, std::span<std::byte const> data)
{
	TGE_ASSERT(conversation.id != 0, "Receive on a conversation that has not started");

	return Call(conversation, ECallback::Receive, data, nullptr);
}

//////////////////////////////////////////////////////////////////////////
std::expected<SScriptAction, std::string> CProtocolScript::Finish(SConversation const& conversation)
{
	TGE_ASSERT(conversation.kind == EConversationKind::Server && conversation.id != 0, "Finish on a server conversation that has not started");

	std::expected<SScriptAction, std::string> result{};

	if (m_serverFinish != 0)
	{
		result = Call(conversation, ECallback::Finish, {}, nullptr);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CProtocolScript::End(SConversation& conversation)
{
	if (conversation.id != 0)
	{
		SEndCall call{ m_states, conversation.id };
		std::string const failure{ Run(&EndConversation, &call) };

		if (!failure.empty())
		{
			TGE_FATAL("Releasing a conversation's state failed");
		}

		conversation.id = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// Lua's allocator: a shrink or free never fails, as Lua requires; growth beyond the cap fails, and so does a large
// growth once the call is out of time.
void* CProtocolScript::Allocate(void* pUserData, void* pBlock, size_t oldSize, size_t newSize)
{
	CProtocolScript& script{ *static_cast<CProtocolScript*>(pUserData) };
	size_t const usedSize{ (pBlock != nullptr) ? oldSize : 0 };
	bool const isGrowing{ newSize > usedSize };
	void* pResult{ nullptr };

	if (newSize == 0)
	{
		std::free(pBlock);
		script.m_numBytes -= usedSize;
	}
	else if (isGrowing && script.m_numBytes + (newSize - usedSize) > MemoryLimit)
	{
		pResult = nullptr;
	}
	else if (isGrowing && newSize - usedSize >= LargeAllocation && std::chrono::steady_clock::now() > script.m_deadline)
	{
		script.m_hasRunOutOfTime = true;
	}
	else
	{
		pResult = std::realloc(pBlock, newSize);

		if (pResult != nullptr)
		{
			script.m_numBytes = script.m_numBytes - usedSize + newSize;
		}
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
void CProtocolScript::CheckDeadline(lua_State* pState, lua_Debug*)
{
	void* pUserData{ nullptr };

	lua_getallocf(pState, &pUserData);

	CProtocolScript& script{ *static_cast<CProtocolScript*>(pUserData) };

	if (std::chrono::steady_clock::now() > script.m_deadline)
	{
		script.m_hasRunOutOfTime = true;
		luaL_where(pState, 0);
		lua_pushliteral(pState, "the script ran out of time");
		lua_concat(pState, 2);
		lua_error(pState);
	}
}

//////////////////////////////////////////////////////////////////////////
std::string CProtocolScript::Run(int (*pBody)(lua_State*), void* pCall)
{
	TGE_ASSERT(m_pState != nullptr && lua_gettop(m_pState) == 0, "A script call needs an initialized script and an empty stack");

	std::string failure{};

	m_deadline = std::chrono::steady_clock::now() + CallTimeLimit;
	m_hasRunOutOfTime = false;
	lua_pushcfunction(m_pState, pBody);
	lua_pushlightuserdata(m_pState, pCall);

	int const status{ lua_pcall(m_pState, 1, 0, 0) };

	if (status == LUA_ERRMEM && m_hasRunOutOfTime)
	{
		failure = "the script ran out of time";
	}
	else if (status != LUA_OK && lua_type(m_pState, -1) == LUA_TSTRING)
	{
		size_t length{ 0 };
		char const* const pText{ lua_tolstring(m_pState, -1, &length) };

		failure.assign(pText, length);
	}
	else if (status != LUA_OK)
	{
		failure = std::format("the script raised a {} as its error", luaL_typename(m_pState, -1));
	}

	lua_settop(m_pState, 0);

	return failure;
}

//////////////////////////////////////////////////////////////////////////
std::expected<SScriptAction, std::string> CProtocolScript::Call(SConversation const& conversation, ECallback callback, std::span<std::byte const> data,
	std::map<std::string, std::string> const* pOptions)
{
	std::array<int, 3> const masterFunctions{ m_masterStart, m_masterReceive, 0 };
	std::array<int, 3> const serverFunctions{ m_serverStart, m_serverReceive, m_serverFinish };
	size_t const kindIndex{ static_cast<size_t>(conversation.kind) };
	size_t const callbackIndex{ static_cast<size_t>(callback) };
	SConversationCall call{};

	call.kind = conversation.kind;
	call.callback = callback;
	call.isStream = conversation.kind == EConversationKind::Master && m_masterTransport == EMasterTransport::Tcp;
	call.id = conversation.id;
	call.states = m_states;
	call.function = (conversation.kind == EConversationKind::Master) ? masterFunctions[callbackIndex] : serverFunctions[callbackIndex];
	call.pOptions = pOptions;
	call.data = data;

	std::string failure{ Run(&CallConversation, &call) };

	if (failure.empty())
	{
		failure = std::move(call.problem);
	}

	std::expected<SScriptAction, std::string> result{ std::move(call.action) };

	if (!failure.empty())
	{
		result = std::unexpected{ std::format("{}.{}: {}", KindNames[kindIndex], CallbackNames[callbackIndex], failure) };
	}

	return result;
}
} // namespace Lkt::Script
