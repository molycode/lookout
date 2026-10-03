#include "launch/game_launcher.hpp"
#include "connect_args.hpp"
#include "loggers.hpp"
#include "query/game_definition.hpp"
#include "query/server_address.hpp"
#include <tge/assert.hpp>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace Lkt::Launch
{
namespace
{
constexpr char const* DevNull{ "/dev/null" };
// A developer console can echo the password the game was given.
constexpr mode_t LogFileMode{ 0600 };
constexpr int FirstUnusedDescriptor{ STDERR_FILENO + 1 };

//////////////////////////////////////////////////////////////////////////
std::string ErrorText(int error)
{
	return std::error_code{ error, std::generic_category() }.message();
}

//////////////////////////////////////////////////////////////////////////
std::string SignalName(int signal)
{
	char const* const pName{ ::sigabbrev_np(signal) };

	return (pName != nullptr) ? std::format("SIG{}", pName) : std::format("signal {}", signal);
}

//////////////////////////////////////////////////////////////////////////
// Opened before the spawn rather than by it, so a log that cannot be written costs the output, not the launch.
int OpenGameLog(std::filesystem::path const& path)
{
	int const fd{ ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, LogFileMode) };

	if (fd < 0)
	{
		gLog.Warning("Cannot open the game log '{}', so the game's output is discarded: {}", path.string(), ErrorText(errno));
	}

	return fd;
}

//////////////////////////////////////////////////////////////////////////
std::string LogHint(SChildProcess const& child)
{
	return child.logPath.empty() ? std::string{} : std::format("; its output is in '{}'", child.logPath);
}

//////////////////////////////////////////////////////////////////////////
// Waits on this pid alone: waitpid(-1) would reap children that other code in the process is waiting for.
bool ReapIfFinished(SChildProcess const& child)
{
	int status{ 0 };
	pid_t const waited{ ::waitpid(child.pid, &status, WNOHANG) };
	int const waitError{ errno };
	bool const hasEnded{ waited == child.pid };
	bool const isLost{ waited < 0 && waitError != EINTR };

	if (hasEnded && WIFEXITED(status) && WEXITSTATUS(status) != 0)
	{
		gLog.Warning("{} exited with status {}{}", child.gameName, WEXITSTATUS(status), LogHint(child));
	}
	else if (hasEnded && WIFSIGNALED(status))
	{
		gLog.Warning("{} was ended by {}{}", child.gameName, SignalName(WTERMSIG(status)), LogHint(child));
	}
	else if (isLost)
	{
		gLog.Warning("Lost track of {} (pid {}): {}", child.gameName, child.pid, ErrorText(waitError));
	}

	return hasEnded || isLost;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CGameLauncher::Initialize(std::string_view logsDir)
{
	m_logsDir = logsDir;
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, ELaunchError> CGameLauncher::Launch(Query::SGameDefinition const& game, SLaunchOption const& option, SConnectRequest const& request)
{
	TGE_ASSERT(!option.argv.empty(), "A launch option without a program");

	std::expected<void, ELaunchError> result{};
	std::expected<std::vector<std::string>, ELaunchError> const connectArgs{ BuildConnectArgs(request) };

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
void CGameLauncher::ReapFinished()
{
	std::erase_if(m_children, ReapIfFinished);
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, ELaunchError> CGameLauncher::Spawn(Query::SGameDefinition const& game, SLaunchOption const& option, SConnectRequest const& request, std::vector<std::string> const& arguments)
{
	std::expected<void, ELaunchError> result{};
	std::filesystem::path const logPath{ m_logsDir.empty() ? std::filesystem::path{} : m_logsDir / std::format("game-{}.log", game.key) };
	int const logFd{ logPath.empty() ? -1 : OpenGameLog(logPath) };
	std::vector<char*> argv{};
	posix_spawn_file_actions_t actions{};
	posix_spawnattr_t attributes{};
	sigset_t noSignals{};
	sigset_t allSignals{};
	pid_t pid{ 0 };

	for (std::string const& argument : arguments)
	{
		argv.emplace_back(const_cast<char*>(argument.c_str()));
	}

	argv.emplace_back(nullptr);
	sigemptyset(&noSignals);
	sigfillset(&allSignals);

	int error{ posix_spawn_file_actions_init(&actions) };
	bool const hasActions{ error == 0 };

	if (hasActions)
	{
		error = posix_spawnattr_init(&attributes);
	}

	bool const hasAttributes{ hasActions && error == 0 };

	if (hasAttributes)
	{
		auto const apply{ [&error](int code)
		{
			error = (error == 0) ? code : error;
		} };

		// Output first: with Lookout's stdin closed the log can be descriptor 0, which opening stdin replaces.
		if (logFd >= 0)
		{
			apply(posix_spawn_file_actions_adddup2(&actions, logFd, STDOUT_FILENO));
		}
		else
		{
			apply(posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, DevNull, O_WRONLY, 0));
		}

		apply(posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO));
		apply(posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, DevNull, O_RDONLY, 0));

		if (!option.workingDir.empty())
		{
			apply(posix_spawn_file_actions_addchdir_np(&actions, option.workingDir.c_str()));
		}

		apply(posix_spawn_file_actions_addclosefrom_np(&actions, FirstUnusedDescriptor));
		apply(posix_spawnattr_setflags(&attributes, static_cast<short>(POSIX_SPAWN_SETSID | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF)));
		apply(posix_spawnattr_setsigmask(&attributes, &noSignals));
		// SDL ignores SIGPIPE, and an ignored SIGCHLD would break the games' launch scripts.
		apply(posix_spawnattr_setsigdefault(&attributes, &allSignals));

		// Discovered launchers are absolute; only a custom command is looked up through PATH.
		if (error == 0)
		{
			error = option.argv.front().contains('/')
				? posix_spawn(&pid, argv.front(), &actions, &attributes, argv.data(), environ)
				: posix_spawnp(&pid, argv.front(), &actions, &attributes, argv.data(), environ);
		}
	}

	if (hasAttributes)
	{
		posix_spawnattr_destroy(&attributes);
	}

	if (hasActions)
	{
		posix_spawn_file_actions_destroy(&actions);
	}

	if (logFd >= 0)
	{
		::close(logFd);
	}

	if (error == 0)
	{
		m_children.emplace_back(pid, std::string{ game.name }, (logFd >= 0) ? logPath.string() : std::string{});
		gLog.Info("Starting {} for {} through {} · {}: {}{}", game.name, Query::FormatAddress(request.address), option.name, option.location, option.argv.front(),
			request.password.empty() ? "" : " (with a password)");
	}
	else
	{
		std::string const where{ option.workingDir.empty() ? std::string{} : std::format(" in '{}'", option.workingDir) };

		gLog.Error("Cannot start {} with '{}'{}: {}", game.name, option.argv.front(), where, ErrorText(error));
		result = std::unexpected{ ELaunchError::SpawnFailed };
	}

	return result;
}
} // namespace Lkt::Launch
