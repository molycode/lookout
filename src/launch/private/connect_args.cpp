#include "connect_args.hpp"
#include "query/server_address.hpp"
#include <algorithm>
#include <string_view>

namespace Lkt::Launch
{
namespace
{
constexpr char FirstPrintable{ 0x20 };
constexpr char LastPrintable{ 0x7E };
constexpr std::string_view AddressPlaceholder{ "{address}" };
constexpr std::string_view PasswordPlaceholder{ "{password}" };

//////////////////////////////////////////////////////////////////////////
bool CanCarry(Query::SPasswordRules const& rules, std::string_view password)
{
	bool const isPrintable{ std::ranges::all_of(password, [](char character)
	{
		return character >= FirstPrintable && character <= LastPrintable;
	}) };

	return password.size() <= rules.maxLength && isPrintable && password.find_first_of(rules.refusedCharacters) == std::string_view::npos
		&& std::ranges::none_of(rules.refusedSequences, [password](std::string const& sequence) { return password.contains(sequence); });
}

//////////////////////////////////////////////////////////////////////////
// One pass, so a password that spells a placeholder is never expanded.
std::vector<std::string> Fill(std::vector<std::string> const& arguments, std::string_view address, std::string_view password)
{
	std::vector<std::string> filled{};

	for (std::string const& argument : arguments)
	{
		std::string& text{ filled.emplace_back() };
		size_t index{ 0 };

		while (index < argument.size())
		{
			std::string_view const rest{ std::string_view{ argument }.substr(index) };

			if (rest.starts_with(AddressPlaceholder))
			{
				text += address;
				index += AddressPlaceholder.size();
			}
			else if (rest.starts_with(PasswordPlaceholder))
			{
				text += password;
				index += PasswordPlaceholder.size();
			}
			else
			{
				text += argument[index];
				++index;
			}
		}
	}

	return filled;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::string>, ELaunchError> BuildConnectArgs(Query::SJoinCommand const& join, SConnectRequest const& request)
{
	std::expected<std::vector<std::string>, ELaunchError> result{ std::unexpected{ ELaunchError::UnsupportedPassword } };
	std::string const address{ Query::FormatAddress(request.address) };

	if (request.password.empty())
	{
		result = Fill(join.arguments, address, {});
	}
	else if (CanCarry(join.password, request.password))
	{
		result = Fill(join.passwordArguments, address, request.password);
	}

	return result;
}
} // namespace Lkt::Launch
