#include "connect_args.hpp"
#include "query/server_address.hpp"
#include <algorithm>

namespace Lkt::Launch
{
namespace
{
// Info_SetValueForKey refuses 64 characters or more, so a longer password would never reach the server.
constexpr size_t MaxPasswordLength{ 63 };
constexpr char FirstPrintable{ 0x21 };
constexpr char LastPrintable{ 0x7E };

// kpded2 rebuilds "set password %s", so a space splits the value; iortcw cuts the command line at every '+' and ends
// a token at "//" or "/*"; both refuse '"', '\' and ';' in userinfo, and Quake 2 expands '$'.
constexpr std::string_view RefusedCharacters{ "\"$;+\\" };

//////////////////////////////////////////////////////////////////////////
bool CanCarry(std::string_view password)
{
	bool const isPrintable{ std::ranges::all_of(password, [](char character)
	{
		return character >= FirstPrintable && character <= LastPrintable;
	}) };

	return password.size() <= MaxPasswordLength && isPrintable
		&& password.find_first_of(RefusedCharacters) == std::string_view::npos
		&& !password.contains("//") && !password.contains("/*");
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Each its own argument: run-game.sh recognises a join by " +connect ".
std::expected<std::vector<std::string>, ELaunchError> BuildConnectArgs(SConnectRequest const& request)
{
	std::expected<std::vector<std::string>, ELaunchError> result{ std::unexpected{ ELaunchError::UnsupportedPassword } };
	std::string const address{ Query::FormatAddress(request.address) };

	if (request.password.empty())
	{
		result = std::vector<std::string>{ "+connect", address };
	}
	else if (CanCarry(request.password))
	{
		result = std::vector<std::string>{ "+set", "password", request.password, "+connect", address };
	}

	return result;
}
} // namespace Lkt::Launch
