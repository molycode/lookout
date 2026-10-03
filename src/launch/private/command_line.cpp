#include "command_line.hpp"
#include "launch/quote_argument.hpp"
#include <optional>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view Separators{ " \t\n" };
constexpr std::string_view QuotedEscapes{ "\"`$\\" };
constexpr std::string_view ShellCharacters{ "'~$`;|&<>()*?#" };
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
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<std::vector<std::string>, ECommandLineError> SplitCommandLine(std::string_view text, EQuoting quoting)
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
		else if (Separators.contains(character))
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
		else if ((quoting == EQuoting::Strict && ShellCharacters.contains(character)) || character == '\'')
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

	std::expected<std::vector<std::string>, ECommandLineError> result{ std::move(arguments) };

	if (error.has_value())
	{
		result = std::unexpected{ *error };
	}

	return result;
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

	std::expected<std::vector<std::string>, ECommandLineError> result{ std::move(expanded) };

	if (error.has_value())
	{
		result = std::unexpected{ *error };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::string QuoteArgument(std::string_view text)
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
} // namespace Lkt::Launch
