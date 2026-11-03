#include "launch/game_launcher.hpp"
#include "connect_args.hpp"
#include "loggers.hpp"
#include "query/game_definition.hpp"
#include "query/server_address.hpp"
#include <tge/assert.hpp>
#include <tge/process/exit_status.hpp>
#include <tge/process/output_file.hpp>
#include <tge/process/process_spec.hpp>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace Lkt::Launch
{
namespace
{
// Windows reports a crash as a status such as 0xC0000005, which reads only in hex.
constexpr uint32_t LargestDecimalStatus{ 0xFFFF };

//////////////////////////////////////////////////////////////////////////
// Opened before the start rather than by it, so a log that cannot be written costs the output, not the launch.
bool OpenGameLog(Tge::Process::COutputFile& log, std::filesystem::path const& path)
{
	std::expected<void, std::error_code> const opened{ log.Open(path) };

	if (!opened.has_value())
	{
		gLog.Warning("Cannot open the game log '{}', so the game's output is discarded: {}", path.string(), opened.error().message());
	}

	return opened.has_value();
}

//////////////////////////////////////////////////////////////////////////
ELaunchError ToLaunchError(std::error_code const& error, std::string_view program)
{
	ELaunchError launchError{ ELaunchError::SpawnFailed };

	if (error == std::errc::no_such_file_or_directory)
	{
		launchError = std::filesystem::path{ program }.has_parent_path() ? ELaunchError::ProgramNotFound : ELaunchError::ProgramNotInPath;
	}
	else if (error == std::errc::executable_format_error || error == std::errc::is_a_directory)
	{
		launchError = ELaunchError::ProgramNotRunnable;
	}
	else if (error == std::errc::permission_denied)
	{
		launchError = ELaunchError::ProgramAccessDenied;
	}

	return launchError;
}

//////////////////////////////////////////////////////////////////////////
std::string FormatStatus(uint32_t status)
{
	return (status > LargestDecimalStatus) ? std::format("0x{:08X}", status) : std::format("{}", status);
}

//////////////////////////////////////////////////////////////////////////
std::string LogHint(SChildProcess const& child)
{
	return child.logPath.empty() ? std::string{} : std::format("; its output is in '{}'", child.logPath);
}

//////////////////////////////////////////////////////////////////////////
bool ReapIfFinished(SChildProcess& child)
{
	std::optional<std::expected<Tge::Process::SExitStatus, std::error_code>> const exit{ child.process.TryGetExit() };
	bool const hasExited{ exit.has_value() && exit->has_value() };

	if (hasExited && !(*exit)->signal.empty())
	{
		gLog.Warning("{} was ended by {}{}", child.gameName, (*exit)->signal, LogHint(child));
	}
	else if (hasExited && (*exit)->code != 0)
	{
		gLog.Warning("{} exited with status {}{}", child.gameName, FormatStatus((*exit)->code), LogHint(child));
	}
	else if (exit.has_value() && !hasExited)
	{
		gLog.Warning("Lost track of {} (pid {}): {}", child.gameName, child.process.GetId(), exit->error().message());
	}

	if (exit.has_value())
	{
		child.process.Terminate();
	}

	return exit.has_value();
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CGameLauncher::Initialize(std::string_view logsDir)
{
	m_logsDir = logsDir;
}

//////////////////////////////////////////////////////////////////////////
// The games keep running; only what Lookout holds of them goes.
void CGameLauncher::Terminate()
{
	for (std::unique_ptr<SChildProcess> const& pChild : m_children)
	{
		pChild->process.Terminate();
	}

	m_children.clear();
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, ELaunchError> CGameLauncher::Launch(Query::SGameDefinition const& game, SLaunchOption const& option, SConnectRequest const& request)
{
	TGE_ASSERT(!option.argv.empty(), "A launch option without a program");

	std::expected<void, ELaunchError> result{};
	std::expected<std::vector<std::string>, ELaunchError> const connectArgs{ BuildConnectArgs(game.join, request) };

	if (connectArgs.has_value())
	{
		std::vector<std::string> arguments{ option.argv };

		arguments.insert(arguments.end(), connectArgs->begin(), connectArgs->end());
		result = Spawn(game, option, request, arguments);
	}
	else
	{
		gLog.Warning("Cannot join {} with {}: {}", Query::FormatAddress(request.address), game.name, ToString(connectArgs.error()));
		result = std::unexpected{ connectArgs.error() };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
size_t CGameLauncher::ReapFinished()
{
	std::erase_if(m_children, [](std::unique_ptr<SChildProcess> const& pChild) { return ReapIfFinished(*pChild); });

	return m_children.size();
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, ELaunchError> CGameLauncher::Spawn(Query::SGameDefinition const& game, SLaunchOption const& option, SConnectRequest const& request,
	std::vector<std::string> const& arguments)
{
	std::expected<void, ELaunchError> result{};
	std::filesystem::path const logPath{ m_logsDir.empty() ? std::filesystem::path{} : m_logsDir / std::format("game-{}.log", game.key) };
	Tge::Process::COutputFile log{};
	bool const hasLog{ !logPath.empty() && OpenGameLog(log, logPath) };
	Tge::Process::SProcessSpec spec{};
	auto pChild{ std::make_unique<SChildProcess>() };

	spec.program = arguments.front();
	spec.arguments.assign(arguments.begin() + 1, arguments.end());
	spec.workingDir = option.workingDir;
	spec.pOutput = hasLog ? &log : nullptr;

	std::expected<void, std::error_code> const started{ pChild->process.Start(spec) };

	log.Close();

	if (started.has_value())
	{
		pChild->gameName = game.name;
		pChild->logPath = hasLog ? logPath.string() : std::string{};
		m_children.emplace_back(std::move(pChild));
		gLog.Info("Starting {} for {} with {} · {}: {}{}", game.name, Query::FormatAddress(request.address), option.name, option.location, option.argv.front(),
			request.password.empty() ? "" : " (with a password)");
	}
	else
	{
		std::string const where{ option.workingDir.empty() ? std::string{} : std::format(" in '{}'", option.workingDir) };

		gLog.Error("Cannot start {} with '{}'{}: {}", game.name, option.argv.front(), where, started.error().message());
		result = std::unexpected{ ToLaunchError(started.error(), spec.program.string()) };
	}

	return result;
}
} // namespace Lkt::Launch
