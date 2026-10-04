#pragma once

#include "script/conversation.hpp"
#include "script/master_transport.hpp"
#include "script/script_action.hpp"
#include "query/protocol_option.hpp"
#include <tge/non_copyable.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
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
enum class ECallback : uint8_t;

// A protocol script in its own sandboxed Lua state. Not thread-safe: one thread uses each instance.
// Each started conversation holds a state table until End; a failed Start leaves none behind.
class CProtocolScript final : private Tge::SNoCopyNoMove
{
public:

	CProtocolScript() = default;
	~CProtocolScript() = default;

	std::expected<void, std::string> Initialize(std::string_view name, std::string_view source);
	void Terminate();

	std::span<Query::SProtocolOption const> GetOptions() const;
	EMasterTransport GetMasterTransport() const;

	// The conversation names its kind and has no id yet; a successful Start gives it one.
	std::expected<SScriptAction, std::string> Start(SConversation& conversation, std::map<std::string, std::string> const& options);
	// Over TCP, empty data means the master closed the connection.
	std::expected<SScriptAction, std::string> Receive(SConversation const& conversation, std::span<std::byte const> data);
	// An empty action when the script has no server.finish.
	std::expected<SScriptAction, std::string> Finish(SConversation const& conversation);
	void End(SConversation& conversation);

private:

	static void* Allocate(void* pUserData, void* pBlock, size_t oldSize, size_t newSize);
	static void CheckDeadline(lua_State* pState, lua_Debug* pDebug);

	std::string Run(int (*pBody)(lua_State*), void* pCall);
	std::expected<SScriptAction, std::string> Call(SConversation const& conversation, ECallback callback, std::span<std::byte const> data,
		std::map<std::string, std::string> const* pOptions);

	lua_State* m_pState{ nullptr };
	std::vector<Query::SProtocolOption> m_options;
	std::chrono::steady_clock::time_point m_deadline{};
	size_t m_numBytes{ 0 };
	uint64_t m_lastConversationId{ 0 };
	int m_states{ 0 };
	int m_masterStart{ 0 };
	int m_masterReceive{ 0 };
	int m_serverStart{ 0 };
	int m_serverReceive{ 0 };
	int m_serverFinish{ 0 };
	EMasterTransport m_masterTransport{ EMasterTransport::Udp };
	bool m_hasRunOutOfTime{ false };
};
} // namespace Lkt::Script
