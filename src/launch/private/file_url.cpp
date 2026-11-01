#include "launch/file_url.hpp"
#include "styled_file_url.hpp"
#include <tge/platform.hpp>
#include <algorithm>
#include <format>
#include <iterator>

namespace Lkt::Launch
{
namespace
{
constexpr std::string_view LongPathPrefix{ "\\\\?\\" };
constexpr std::string_view LongUncPrefix{ "\\\\?\\UNC\\" };

//////////////////////////////////////////////////////////////////////////
constexpr bool IsKeptAsIs(unsigned char byte)
{
	return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9')
		|| byte == '-' || byte == '.' || byte == '_' || byte == '~' || byte == '/';
}

//////////////////////////////////////////////////////////////////////////
// Windows leaves UTF-8 as it is: SDL hands the URL on as UTF-16, and the shell reads it as the path it names.
void AppendEncoded(std::string& url, std::string_view text, EPathStyle style)
{
	for (char const character : text)
	{
		unsigned char const byte{ static_cast<unsigned char>(character) };

		if (IsKeptAsIs(byte) || (style == EPathStyle::Windows && byte >= 0x80))
		{
			url += character;
		}
		else
		{
			std::format_to(std::back_inserter(url), "%{:02X}", byte);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool HasDriveLetter(std::string_view path)
{
	bool const isLetter{ !path.empty() && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) };

	return isLetter && path.size() >= 2 && path[1] == ':';
}

//////////////////////////////////////////////////////////////////////////
// A drive path becomes file:///C:/…, a share file://server/share/…; the long-path prefixes are not part of either.
std::string ToWindowsFileUrl(std::string_view path)
{
	std::string url{ "file://" };
	std::string slashed{};

	if (path.starts_with(LongUncPrefix))
	{
		slashed = std::string{ "\\\\" } + std::string{ path.substr(LongUncPrefix.size()) };
	}
	else
	{
		slashed = path.starts_with(LongPathPrefix) ? path.substr(LongPathPrefix.size()) : path;
	}

	std::ranges::replace(slashed, '\\', '/');

	if (slashed.starts_with("//"))
	{
		AppendEncoded(url, std::string_view{ slashed }.substr(2), EPathStyle::Windows);
	}
	else if (HasDriveLetter(slashed))
	{
		url += '/';
		url += slashed.substr(0, 2);
		AppendEncoded(url, std::string_view{ slashed }.substr(2), EPathStyle::Windows);
	}
	else
	{
		AppendEncoded(url, slashed, EPathStyle::Windows);
	}

	return url;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::string ToFileUrl(std::string_view path, EPathStyle style)
{
	std::string url{};

	if (style == EPathStyle::Windows)
	{
		url = ToWindowsFileUrl(path);
	}
	else
	{
		url = "file://";
		AppendEncoded(url, path, EPathStyle::Posix);
	}

	return url;
}

//////////////////////////////////////////////////////////////////////////
std::string ToFileUrl(std::string_view path)
{
#if defined(TGE_PLATFORM_WINDOWS)
	return ToFileUrl(path, EPathStyle::Windows);
#elif defined(TGE_PLATFORM_LINUX)
	return ToFileUrl(path, EPathStyle::Posix);
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
}
} // namespace Lkt::Launch
