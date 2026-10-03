#include "launch/file_url.hpp"
#include <format>
#include <iterator>

namespace Lkt::Launch
{
namespace
{
//////////////////////////////////////////////////////////////////////////
constexpr bool IsKeptAsIs(unsigned char byte)
{
	return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9')
		|| byte == '-' || byte == '.' || byte == '_' || byte == '~' || byte == '/';
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::string ToFileUrl(std::string_view path)
{
	std::string url{ "file://" };

	for (char const character : path)
	{
		unsigned char const byte{ static_cast<unsigned char>(character) };

		if (IsKeptAsIs(byte))
		{
			url += character;
		}
		else
		{
			std::format_to(std::back_inserter(url), "%{:02X}", byte);
		}
	}

	return url;
}
} // namespace Lkt::Launch
