#include "command_line.hpp"
#include "typed_command_syntax.hpp"
#include "launch/quote_argument.hpp"
#include <algorithm>
#include <optional>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view PosixSeparators{ " \t\n" };
constexpr std::string_view QuotedEscapes{ "\"`$\\" };
constexpr std::string_view PosixShellCharacters{ "'~$`;|&<>()*?#" };
constexpr std::string_view WindowsSeparators{ " \t" };
constexpr std::string_view WindowsShellCharacters{ "&|<>^%" };
constexpr std::string_view RemovedFieldCodes{ "fFuUidDnNvm" };

//////////////////////////////////////////////////////////////////////////
bool IsRemovedFieldCode(std::string_view argument)
{
	return argument.size() == 2 && argument[0] == '%' && RemovedFieldCodes.contains(argument[1]);
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::string, ECommandLineError> ExpandArgument(std::string_view argument, std::string_view name, std::string_view filePath)
{
	std::string expanded{};
	bool isValid{ true };

	for (size_t index{ 0 }; isValid && index < argument.size(); ++index)
	{
		if (argument[index] == '%')
		{
			++index;

			char const code{ (index < argument.size()) ? argument[index] : '\0' };

			if (code == '%')
			{
				expanded += '%';
			}
			else if (code == 'c')
			{
				expanded += name;
			}
			else if (code == 'k')
			{
				expanded += filePath;
			}
			else
			{
				isValid = code != '\0' && RemovedFieldCodes.contains(code);
			}
		}
		else
		{
			expanded += argument[index];
		}
	}

	std::expected<std::string, ECommandLineError> result{ std::unexpected{ ECommandLineError::UnknownFieldCode } };

	if (isValid)
	{
		result = std::move(expanded);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::string>, ECommandLineError> ToResult(std::vector<std::string> arguments, std::optional<ECommandLineError> const& error)
{
	std::expected<std::vector<std::string>, ECommandLineError> result{ std::move(arguments) };

	if (error.has_value())
	{
		result = std::unexpected{ *error };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// The Desktop Entry specification's Exec quoting.
std::expected<std::vector<std::string>, ECommandLineError> SplitPosix(std::string_view text, EQuoting quoting)
{
	std::vector<std::string> arguments{};
	std::string current{};
	std::optional<ECommandLineError> error{};
	bool isInWord{ false };
	bool isQuoted{ false };

	for (size_t index{ 0 }; !error.has_value() && index < text.size(); ++index)
	{
		char const character{ text[index] };

		if (isQuoted)
		{
			if (character == '"')
			{
				isQuoted = false;
			}
			else if (character == '\\')
			{
				++index;

				if (index < text.size() && QuotedEscapes.contains(text[index]))
				{
					current += text[index];
				}
				else
				{
					error = ECommandLineError::StrayBackslash;
				}
			}
			else
			{
				current += character;
			}
		}
		else if (PosixSeparators.contains(character))
		{
			if (isInWord)
			{
				arguments.emplace_back(std::move(current));
				current.clear();
				isInWord = false;
			}
		}
		else if (character == '"')
		{
			isQuoted = true;
			isInWord = true;
		}
		else if (character == '\\')
		{
			error = ECommandLineError::StrayBackslash;
		}
		// A single quote is refused even leniently: GLib would honour it as shell quoting, which this splitting lacks.
		else if ((quoting == EQuoting::Strict && PosixShellCharacters.contains(character)) || character == '\'')
		{
			error = ECommandLineError::UnquotedShellCharacter;
		}
		else
		{
			current += character;
			isInWord = true;
		}
	}

	if (!error.has_value() && isQuoted)
	{
		error = ECommandLineError::UnterminatedQuote;
	}

	if (isInWord)
	{
		arguments.emplace_back(std::move(current));
	}

	return ToResult(std::move(arguments), error);
}

//////////////////////////////////////////////////////////////////////////
// The C runtime's rules, which a Windows program applies to its own command line: backslashes are literal except
// before a quote, and "" inside quotes is a quote.
std::expected<std::vector<std::string>, ECommandLineError> SplitWindows(std::string_view text, EQuoting quoting)
{
	std::vector<std::string> arguments{};
	std::string current{};
	std::optional<ECommandLineError> error{};
	bool isInWord{ false };
	bool isQuoted{ false };
	size_t index{ 0 };

	while (!error.has_value() && index < text.size())
	{
		char const character{ text[index] };

		if (character == '\\')
		{
			size_t const numBackslashes{ std::min(text.find_first_not_of('\\', index), text.size()) - index };
			bool const isBeforeQuote{ index + numBackslashes < text.size() && text[index + numBackslashes] == '"' };

			current.append(isBeforeQuote ? numBackslashes / 2 : numBackslashes, '\\');
			index += numBackslashes;
			isInWord = true;

			if (isBeforeQuote && numBackslashes % 2 == 1)
			{
				current += '"';
				++index;
			}
		}
		else if (character == '"')
		{
			if (isQuoted && index + 1 < text.size() && text[index + 1] == '"')
			{
				current += '"';
				++index;
			}
			else
			{
				isQuoted = !isQuoted;
			}

			isInWord = true;
			++index;
		}
		else if (!isQuoted && WindowsSeparators.contains(character))
		{
			if (isInWord)
			{
				arguments.emplace_back(std::move(current));
				current.clear();
				isInWord = false;
			}

			++index;
		}
		else if (!isQuoted && quoting == EQuoting::Strict && WindowsShellCharacters.contains(character))
		{
			error = ECommandLineError::UnquotedShellCharacter;
		}
		else
		{
			current += character;
			isInWord = true;
			++index;
		}
	}

	if (!error.has_value() && isQuoted)
	{
		error = ECommandLineError::UnterminatedQuote;
	}

	if (isInWord)
	{
		arguments.emplace_back(std::move(current));
	}

	return ToResult(std::move(arguments), error);
}

//////////////////////////////////////////////////////////////////////////
std::string QuotePosix(std::string_view text)
{
	std::string quoted{ '"' };

	for (char const character : text)
	{
		if (QuotedEscapes.contains(character))
		{
			quoted += '\\';
		}

		quoted += character;
	}

	quoted += '"';

	return quoted;
}

//////////////////////////////////////////////////////////////////////////
std::string QuoteWindows(std::string_view text)
{
	std::string quoted{ '"' };
	size_t numBackslashes{ 0 };

	for (char const character : text)
	{
		if (character == '\\')
		{
			++numBackslashes;
		}
		else if (character == '"')
		{
			quoted.append(numBackslashes * 2 + 1, '\\');
			quoted += '"';
			numBackslashes = 0;
		}
		else
		{
			quoted.append(numBackslashes, '\\');
			quoted += character;
			numBackslashes = 0;
		}
	}

	quoted.append(numBackslashes * 2, '\\');
	quoted += '"';

	return quoted;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::string>, ECommandLineError> SplitCommandLine(std::string_view text, ECommandSyntax syntax, EQuoting quoting)
{
	return (syntax == ECommandSyntax::Windows) ? SplitWindows(text, quoting) : SplitPosix(text, quoting);
}

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::string>, ECommandLineError> ExpandFieldCodes(std::vector<std::string> const& arguments, std::string_view name, std::string_view filePath)
{
	std::vector<std::string> expanded{};
	std::optional<ECommandLineError> error{};

	for (size_t index{ 0 }; !error.has_value() && index < arguments.size(); ++index)
	{
		if (!IsRemovedFieldCode(arguments[index]))
		{
			std::expected<std::string, ECommandLineError> argument{ ExpandArgument(arguments[index], name, filePath) };

			if (argument.has_value())
			{
				expanded.emplace_back(std::move(*argument));
			}
			else
			{
				error = argument.error();
			}
		}
	}

	return ToResult(std::move(expanded), error);
}

//////////////////////////////////////////////////////////////////////////
std::string QuoteArgument(std::string_view text, ECommandSyntax syntax)
{
	return (syntax == ECommandSyntax::Windows) ? QuoteWindows(text) : QuotePosix(text);
}

//////////////////////////////////////////////////////////////////////////
std::string QuoteArgument(std::string_view text)
{
	return QuoteArgument(text, GetTypedCommandSyntax());
}
} // namespace Lkt::Launch
