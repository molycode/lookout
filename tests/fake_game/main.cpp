#include <tge/platform.hpp>
#include <array>
#include <charconv>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#if defined(TGE_PLATFORM_WINDOWS)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#elif defined(TGE_PLATFORM_LINUX)
#include <unistd.h>
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)

// [--exit=<code>] [--signal=<number>] [--stderr=<text>] [--cwd-to=<file>] [--pid-to=<file>] arguments...
namespace
{
constexpr std::array<std::string_view, 5> Options{ "--exit=", "--signal=", "--stderr=", "--cwd-to=", "--pid-to=" };

//////////////////////////////////////////////////////////////////////////
bool IsOption(std::string_view arg)
{
	bool isOption{ false };

	for (std::string_view const option : Options)
	{
		isOption = isOption || arg.starts_with(option);
	}

	return isOption;
}

//////////////////////////////////////////////////////////////////////////
void WriteFile(std::string_view path, std::string_view text)
{
	std::ofstream{ std::filesystem::path{ path }, std::ios::binary } << text;
}

//////////////////////////////////////////////////////////////////////////
int GetOwnId()
{
#if defined(TGE_PLATFORM_WINDOWS)
	return _getpid();
#elif defined(TGE_PLATFORM_LINUX)
	return ::getpid();
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
}

//////////////////////////////////////////////////////////////////////////
std::string GetWorkingDirectory()
{
	std::error_code error{};
	std::u8string const text{ std::filesystem::current_path(error).u8string() };

	return std::string{ reinterpret_cast<char const*>(text.data()), text.size() };
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Every argument from the first that is not one of its options goes to stdout, one a line, in binary so a test
// reads the same bytes on every platform; stdout is flushed before stderr, which may be the same file.
int main(int argc, char* argv[])
{
	std::span<char* const> const args{ argv, static_cast<size_t>(argc) };
	int exitCode{ 0 };
	int signal{ 0 };
	std::string errors{};
	bool isOption{ true };

#if defined(TGE_PLATFORM_WINDOWS)
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);
#endif // defined(TGE_PLATFORM_WINDOWS)

	for (std::string_view const arg : args.subspan(1))
	{
		isOption = isOption && IsOption(arg);

		if (!isOption)
		{
			std::fputs(std::format("{}\n", arg).c_str(), stdout);
		}
		else if (arg.starts_with("--exit="))
		{
			std::from_chars(arg.data() + 7, arg.data() + arg.size(), exitCode);
		}
		else if (arg.starts_with("--signal="))
		{
			std::from_chars(arg.data() + 9, arg.data() + arg.size(), signal);
		}
		else if (arg.starts_with("--stderr="))
		{
			errors += std::format("{}\n", arg.substr(9));
		}
		else if (arg.starts_with("--cwd-to="))
		{
			WriteFile(arg.substr(9), GetWorkingDirectory());
		}
		else if (arg.starts_with("--pid-to="))
		{
			WriteFile(arg.substr(9), std::format("{}", GetOwnId()));
		}
	}

	std::fflush(stdout);
	std::fputs(errors.c_str(), stderr);
	std::fflush(stderr);

	if (signal != 0)
	{
		std::raise(signal);
	}

	return exitCode;
}
