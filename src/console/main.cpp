#include "cli/session.hpp"
#include <tge/process/child_process.hpp>
#include <tge/process/get_program_path.hpp>
#include <tge/process/process_spec.hpp>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <system_error>

namespace
{
//////////////////////////////////////////////////////////////////////////
// The window opens and the prompt returns, as when a window program is typed at a prompt. The window runs in its own
// folder, not the prompt's, which it would keep from being removed.
int StartWindow()
{
	std::expected<std::filesystem::path, std::error_code> const program{ Tge::Process::GetProgramPath() };
	std::filesystem::path const folder{ program.value_or(std::filesystem::path{}).parent_path() };
	std::filesystem::path const window{ folder / "lookout.exe" };
	Tge::Process::CChildProcess child{};
	std::expected<void, std::error_code> const started{ program.has_value()
		? child.Start(Tge::Process::SProcessSpec{ .program = window, .workingDir = folder }) : std::unexpected{ program.error() } };

	child.Terminate();

	if (!started.has_value())
	{
		std::println(stderr, "Cannot start Lookout's window, {}: {}", window.string(), started.error().message());
	}

	return started.has_value() ? EXIT_SUCCESS : EXIT_FAILURE;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	std::span<char* const> const args{ argv, static_cast<size_t>(argc) };
	int result{ EXIT_FAILURE };

	if (args.size() > 1)
	{
		Lkt::Cli::CSession session{};

		session.Initialize();
		result = session.Run(args, {});
		session.Terminate();
	}
	else
	{
		result = StartWindow();
	}

	return result;
}
