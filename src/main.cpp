#include "lookout.hpp"
#include "cli/session.hpp"
#include <tge/platform.hpp>
#include <cstdlib>
#include <span>

#if defined(TGE_PLATFORM_WINDOWS)
#include "ui/show_error.hpp"
#include <format>
#include <string>
#include <string_view>

namespace
{
constexpr std::string_view CommandLineNotice{ "lookout.exe opens Lookout's window and takes no options.\n\nLookout's command line "
	"is lookout.com, in the same folder. Command Prompt and PowerShell run it for \"lookout\"; Git Bash needs "
	"\"lookout.com\":\n\nlookout.com --help" };
} // namespace
#endif // defined(TGE_PLATFORM_WINDOWS)

//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	std::span<char* const> const args{ argv, static_cast<size_t>(argc) };
	Lkt::Cli::CSession session{};
	Lkt::CLookout lookout{};
	auto const runWindow{ [&lookout](Lkt::Cli::CSession& running) { return lookout.Run(running); } };
	int result{ EXIT_FAILURE };

#if defined(TGE_PLATFORM_WINDOWS)
	// A window program has no console, so the C runtime shows its own failures rather than writing them nowhere.
	_set_error_mode(_OUT_TO_MSGBOX);

	if (args.size() > 1)
	{
		Lkt::Ui::ShowError(CommandLineNotice);
	}
	else
	{
		session.Initialize();

		if (session.RunWindow(runWindow))
		{
			result = EXIT_SUCCESS;
		}
		else
		{
			std::string_view const logsDir{ session.GetLogsDir() };

			Lkt::Ui::ShowError(logsDir.empty() ? std::string{ "Lookout could not start." } : std::format("Lookout could not start. Its log in {} says why.", logsDir));
		}

		session.Terminate();
	}
#elif defined(TGE_PLATFORM_LINUX)
	session.Initialize();
	result = session.Run(args, runWindow);
	session.Terminate();
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

	return result;
}
