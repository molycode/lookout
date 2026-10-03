#include "status_body.hpp"
#include <charconv>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
bool ParseInfoString(std::string_view info, std::vector<SRule>& rules)
{
	bool valid{ info.starts_with('\\') };
	size_t start{ 1 };

	while (valid && start < info.size())
	{
		size_t const keyEnd{ info.find('\\', start) };

		valid = keyEnd != std::string_view::npos;

		if (valid)
		{
			size_t const valueEnd{ info.find('\\', keyEnd + 1) };
			size_t const valueLength{ (valueEnd == std::string_view::npos) ? std::string_view::npos : valueEnd - keyEnd - 1 };

			rules.emplace_back(std::string{ info.substr(start, keyEnd - start) }, std::string{ info.substr(keyEnd + 1, valueLength) });
			start = (valueEnd == std::string_view::npos) ? info.size() : valueEnd + 1;
		}
	}

	return valid;
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
bool ParseNumberField(std::string_view& text, T& value)
{
	size_t const start{ text.find_first_not_of(' ') };
	bool valid{ start != std::string_view::npos };

	if (valid)
	{
		char const* const pBegin{ text.data() + start };
		std::from_chars_result const result{ std::from_chars(pBegin, text.data() + text.size(), value) };

		valid = result.ec == std::errc{} && result.ptr != pBegin;
		text.remove_prefix(static_cast<size_t>(result.ptr - text.data()));
	}

	return valid;
}

//////////////////////////////////////////////////////////////////////////
// <score> <ping> "<name>"; some mods put more numbers before the name, so the name is what the quotes enclose.
bool ParsePlayerLine(std::string_view line, SPlayer& player)
{
	size_t const nameStart{ line.find('"') };
	size_t const nameEnd{ line.rfind('"') };
	std::string_view numbers{ line.substr(0, nameStart) };

	bool const valid{ nameStart != std::string_view::npos && nameEnd > nameStart
		&& ParseNumberField(numbers, player.score) && ParseNumberField(numbers, player.ping) };

	if (valid)
	{
		player.name = line.substr(nameStart + 1, nameEnd - nameStart - 1);
	}

	return valid;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<SStatusReply, EParseError> ParseStatusBody(std::string_view body)
{
	std::expected<SStatusReply, EParseError> result{ std::unexpected{ EParseError::Malformed } };

	size_t const infoEnd{ body.find('\n') };
	SStatusReply reply{};

	if (ParseInfoString(body.substr(0, infoEnd), reply.rules))
	{
		std::string_view lines{ (infoEnd == std::string_view::npos) ? std::string_view{} : body.substr(infoEnd + 1) };

		while (!lines.empty())
		{
			size_t const lineEnd{ lines.find('\n') };
			std::string_view const line{ lines.substr(0, lineEnd) };

			if (!line.empty())
			{
				SPlayer player{};

				if (ParsePlayerLine(line, player))
				{
					reply.players.emplace_back(std::move(player));
				}
				else
				{
					++reply.numMalformedPlayerLines;
				}
			}

			lines.remove_prefix((lineEnd == std::string_view::npos) ? lines.size() : lineEnd + 1);
		}

		result = std::move(reply);
	}

	return result;
}
} // namespace Lkt::Query
