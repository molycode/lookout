#pragma once

#include "query/parse_error.hpp"
#include "query/protocol_option.hpp"
#include "query/server_address.hpp"
#include "query/status_reply.hpp"
#include <tge/non_copyable.hpp>
#include <chrono>
#include <cstddef>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;
struct lua_Debug;

namespace Lkt::Script
{
// A protocol script in its own sandboxed Lua state. Not thread-safe: one thread uses each instance.
class CProtocolScript final : private Tge::SNoCopyNoMove
{
public:

	CProtocolScript() = default;
	~CProtocolScript() = default;

	std::expected<void, std::string> Initialize(std::string_view name, std::string_view source);
	void Terminate();

	std::span<Query::SProtocolOption const> GetOptions() const;

	std::expected<std::vector<std::byte>, std::string> MasterRequest(std::map<std::string, std::string> const& options);
	std::expected<std::vector<std::byte>, std::string> StatusRequest(std::map<std::string, std::string> const& options);

	// On ScriptFailed, GetLastFailure() says why.
	std::expected<void, Query::EParseError> ParseMasterReply(std::span<std::byte const> datagram, std::vector<Query::SServerAddress>& servers);
	std::expected<Query::SStatusReply, Query::EParseError> ParseStatusReply(std::span<std::byte const> datagram);
	std::string_view GetLastFailure() const;

private:

	static void* Allocate(void* pUserData, void* pBlock, size_t oldSize, size_t newSize);
	static void CheckDeadline(lua_State* pState, lua_Debug* pDebug);

	std::string Run(int (*pBody)(lua_State*), void* pCall);
	std::expected<std::vector<std::byte>, std::string> Request(int function, char const* pFunctionName, std::map<std::string, std::string> const& options);

	lua_State* m_pState{ nullptr };
	std::vector<Query::SProtocolOption> m_options;
	std::string m_lastFailure;
	std::chrono::steady_clock::time_point m_deadline{};
	size_t m_numBytes{ 0 };
	int m_masterRequest{ 0 };
	int m_statusRequest{ 0 };
	int m_parseMasterReply{ 0 };
	int m_parseStatusReply{ 0 };
	bool m_hasRunOutOfTime{ false };
};
} // namespace Lkt::Script
