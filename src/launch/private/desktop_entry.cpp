#include "desktop_entry.hpp"
#include <optional>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view DesktopEntryGroup{ "[Desktop Entry]" };
constexpr std::string_view Blanks{ " \t" };

//////////////////////////////////////////////////////////////////////////
std::string_view TrimStart(std::string_view text)
{
	size_t const start{ text.find_first_not_of(Blanks) };

	return (start != std::string_view::npos) ? text.substr(start) : std::string_view{};
}

//////////////////////////////////////////////////////////////////////////
std::string_view TrimEnd(std::string_view text)
{
	size_t const end{ text.find_last_not_of(Blanks) };

	return (end != std::string_view::npos) ? text.substr(0, end + 1) : std::string_view{};
}

//////////////////////////////////////////////////////////////////////////
// GLib refuses the whole key on an escape it does not know, and an entry GLib refuses is no launcher either.
std::expected<std::string, EDesktopEntryError> DecodeString(std::string_view value)
{
	std::string decoded{};
	bool isValid{ true };

	for (size_t index{ 0 }; isValid && index < value.size(); ++index)
	{
		char const character{ value[index] };

		if (character == '\\')
		{
			++index;

			switch ((index < value.size()) ? value[index] : '\0')
			{
				case 's':
					decoded += ' ';
					break;
				case 'n':
					decoded += '\n';
					break;
				case 't':
					decoded += '\t';
					break;
				case 'r':
					decoded += '\r';
					break;
				case '\\':
					decoded += '\\';
					break;
				default:
					isValid = false;
					break;
			}
		}
		else
		{
			decoded += character;
		}
	}

	std::expected<std::string, EDesktopEntryError> result{ std::unexpected{ EDesktopEntryError::UnknownEscape } };

	if (isValid)
	{
		result = std::move(decoded);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::expected<bool, EDesktopEntryError> DecodeBoolean(std::string_view value)
{
	std::expected<bool, EDesktopEntryError> result{ std::unexpected{ EDesktopEntryError::InvalidBoolean } };

	if (value == "true" || value == "1")
	{
		result = true;
	}
	else if (value == "false" || value == "0")
	{
		result = false;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::string* FindStringKey(std::string_view key, SDesktopEntry& entry)
{
	std::string* pValue{ nullptr };

	if (key == "Type")
	{
		pValue = &entry.type;
	}
	else if (key == "Name")
	{
		pValue = &entry.name;
	}
	else if (key == "Exec")
	{
		pValue = &entry.exec;
	}
	else if (key == "TryExec")
	{
		pValue = &entry.tryExec;
	}
	else if (key == "Path")
	{
		pValue = &entry.path;
	}

	return pValue;
}

//////////////////////////////////////////////////////////////////////////
// A localised key such as Name[de] matches no key read here, so it is skipped like any other.
std::optional<EDesktopEntryError> ReadKey(std::string_view line, SDesktopEntry& entry)
{
	std::optional<EDesktopEntryError> error{};
	size_t const equals{ line.find('=') };
	std::string_view const key{ TrimEnd(line.substr(0, equals)) };

	if (equals == std::string_view::npos || key.empty())
	{
		error = EDesktopEntryError::MalformedLine;
	}
	else
	{
		std::string_view const value{ TrimStart(line.substr(equals + 1)) };
		std::string* const pValue{ FindStringKey(key, entry) };

		if (pValue != nullptr)
		{
			std::expected<std::string, EDesktopEntryError> decoded{ DecodeString(value) };

			if (decoded.has_value())
			{
				*pValue = std::move(*decoded);
			}
			else
			{
				error = decoded.error();
			}
		}
		else if (key == "Hidden")
		{
			std::expected<bool, EDesktopEntryError> const isHidden{ DecodeBoolean(TrimEnd(value)) };

			if (isHidden.has_value())
			{
				entry.isHidden = *isHidden;
			}
			else
			{
				error = isHidden.error();
			}
		}
	}

	return error;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<SDesktopEntry, EDesktopEntryError> ParseDesktopEntry(std::string_view text)
{
	SDesktopEntry entry{};
	std::optional<EDesktopEntryError> error{};
	bool isInGroup{ false };
	bool hasGroup{ false };
	size_t start{ 0 };

	while (!error.has_value() && start < text.size())
	{
		size_t const newline{ text.find('\n', start) };
		size_t const end{ (newline != std::string_view::npos) ? newline : text.size() };
		std::string_view line{ text.substr(start, end - start) };

		start = end + 1;

		if (line.ends_with('\r'))
		{
			line.remove_suffix(1);
		}

		line = TrimStart(line);

		if (line.starts_with('['))
		{
			isInGroup = TrimEnd(line) == DesktopEntryGroup;
			hasGroup = hasGroup || isInGroup;
		}
		else if (isInGroup && !line.empty() && !line.starts_with('#'))
		{
			error = ReadKey(line, entry);
		}
	}

	std::expected<SDesktopEntry, EDesktopEntryError> result{ std::unexpected{ EDesktopEntryError::NoDesktopEntryGroup } };

	if (error.has_value())
	{
		result = std::unexpected{ *error };
	}
	else if (hasGroup)
	{
		result = std::move(entry);
	}

	return result;
}
} // namespace Lkt::Launch
