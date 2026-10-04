#include "script/protocol_script.hpp"
#include "load_call.hpp"
#include "lua_api.hpp"
#include "parse_call.hpp"
#include "request_call.hpp"
#include "script_calls.hpp"
#include <tge/assert.hpp>
#include <cstdlib>
#include <format>
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
			m_masterRequest = call.masterRequest;
			m_statusRequest = call.statusRequest;
			m_parseMasterReply = call.parseMasterReply;
			m_parseStatusReply = call.parseStatusReply;
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
	m_lastFailure.clear();
}

//////////////////////////////////////////////////////////////////////////
std::span<Query::SProtocolOption const> CProtocolScript::GetOptions() const
{
	return m_options;
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::byte>, std::string> CProtocolScript::MasterRequest(std::map<std::string, std::string> const& options)
{
	return Request(m_masterRequest, "masterRequest", options);
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::byte>, std::string> CProtocolScript::StatusRequest(std::map<std::string, std::string> const& options)
{
	return Request(m_statusRequest, "statusRequest", options);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, Query::EParseError> CProtocolScript::ParseMasterReply(std::span<std::byte const> datagram, std::vector<Query::SServerAddress>& servers)
{
	std::expected<void, Query::EParseError> result{};
	size_t const numKnown{ servers.size() };
	SParseCall call{};

	call.function = m_parseMasterReply;
	call.datagram = datagram;
	call.pServers = &servers;

	std::string failure{ Run(&CallParseMasterReply, &call) };

	if (failure.empty())
	{
		failure = std::move(call.problem);
	}

	if (!failure.empty())
	{
		servers.resize(numKnown);
		m_lastFailure = std::format("parseMasterReply: {}", failure);
		result = std::unexpected{ Query::EParseError::ScriptFailed };
	}
	else if (call.reason.has_value())
	{
		result = std::unexpected{ *call.reason };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<Query::SStatusReply, Query::EParseError> CProtocolScript::ParseStatusReply(std::span<std::byte const> datagram)
{
	Query::SStatusReply reply{};
	SParseCall call{};

	call.function = m_parseStatusReply;
	call.datagram = datagram;
	call.pReply = &reply;

	std::string failure{ Run(&CallParseStatusReply, &call) };

	if (failure.empty())
	{
		failure = std::move(call.problem);
	}

	std::expected<Query::SStatusReply, Query::EParseError> result{ std::move(reply) };

	if (!failure.empty())
	{
		m_lastFailure = std::format("parseStatusReply: {}", failure);
		result = std::unexpected{ Query::EParseError::ScriptFailed };
	}
	else if (call.reason.has_value())
	{
		result = std::unexpected{ *call.reason };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::string_view CProtocolScript::GetLastFailure() const
{
	return m_lastFailure;
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
std::expected<std::vector<std::byte>, std::string> CProtocolScript::Request(int function, char const* pFunctionName, std::map<std::string, std::string> const& options)
{
	SRequestCall call{};

	call.function = function;
	call.pOptions = &options;

	std::string failure{ Run(&CallRequest, &call) };

	if (failure.empty())
	{
		failure = std::move(call.problem);
	}

	std::expected<std::vector<std::byte>, std::string> result{ std::move(call.bytes) };

	if (!failure.empty())
	{
		result = std::unexpected{ std::format("{}: {}", pFunctionName, failure) };
	}

	return result;
}
} // namespace Lkt::Script
