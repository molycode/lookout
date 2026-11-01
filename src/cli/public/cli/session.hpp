#pragma once

#include "net/crypto_threading.hpp"
#include "query/game_problem.hpp"
#include <tge/net/socket_library.hpp>
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Cli
{
struct SRunRequest;

class CSession final : private Tge::SNoCopyNoMove
{
public:

	CSession() = default;
	~CSession() = default;

	void Initialize();
	void Terminate();

	int Run(std::span<char* const> args, std::function<bool(CSession&)> const& window);
	bool RunWindow(std::function<bool(CSession&)> const& window);
	void ReloadGames(Browser::CBrowser& browser);

	std::string_view GetConfigDir() const;
	std::string_view GetLogsDir() const;
	std::filesystem::path const& GetUserDir() const;
	std::filesystem::path const& GetCacheDir() const;
	std::vector<Query::SGameProblem> const& GetGameProblems() const;

private:

	bool RunInRuntime(SRunRequest const& request, std::function<bool(CSession&)> const& window);
	bool RunList(Query::SGameDefinition const& game);
	bool RunDownload(std::span<std::string const> keys);
	void ReportGameProblems(std::span<Query::SGameProblem const> problems) const;
	void StartListLogging();
	void StopListLogging();
	void PrepareDirectories();
	void ReportStartupProblems() const;
	void PruneLogs() const;

	Net::CCryptoThreading m_cryptoThreading{};
	Tge::Net::CSocketLibrary m_socketLibrary{};
	std::string m_logsDir{};
	std::string m_configDir{};
	std::vector<std::string> m_startupProblems{};
	std::filesystem::path m_userDir{};
	std::filesystem::path m_cacheDir{};
	std::vector<Query::SGameProblem> m_gameProblems{};
};
} // namespace Cli
} // namespace Lkt
