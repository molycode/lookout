#pragma once

#include "browser/master_probe_request.hpp"
#include "browser/master_probe_result.hpp"
#include "net/clock.hpp"
#include "net/query_engine.hpp"
#include "net/query_event.hpp"
#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include <tge/non_copyable.hpp>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Browser
{
class CMasterProbes final : private Tge::SNoCopyNoMove
{
public:

	CMasterProbes() = default;
	~CMasterProbes() = default;

	void Initialize(std::function<void()> wake);
	void Terminate();

	void Want(std::span<SMasterProbeRequest const> requests, std::span<Query::SProtocolDefinition const> protocols, bool canStart, Net::Clock::time_point now);
	void Update();
	void Forget(std::string_view key);
	void Clear();

	SMasterProbeResult const* GetResult(std::string_view key) const;
	std::optional<Net::Clock::time_point> GetNextDeadline() const;

private:

	bool IsDue(std::string_view key) const;
	void StartRun(std::span<Query::SProtocolDefinition const> protocols);
	void StopRun();

	std::function<void()> m_wake;
	std::vector<SMasterProbeRequest> m_wanted;
	std::map<std::string, Net::Clock::time_point, std::less<>> m_wantedSince;
	std::map<std::string, SMasterProbeResult, std::less<>> m_results;
	Net::CQueryEngine m_engine;
	std::vector<std::string> m_runKeys;
	std::vector<Query::SGameDefinition> m_runGames;
	std::vector<Query::SProtocolDefinition> m_runProtocols;
	std::vector<Net::SQueryEvent> m_events;
	Net::Clock::time_point m_lastNow{};
	bool m_isRunning{ false };
	bool m_isCleared{ false };
};
} // namespace Lkt::Browser
