#include "registry.hpp"
#include <tge/platform.hpp>

#if defined(TGE_PLATFORM_WINDOWS)
#include "loggers.hpp"
#include <charconv>
#include <cstdint>
#include <cwchar>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <windows.h>

namespace Lkt::Launch
{
namespace
{
// Valve and GOG register in the 32-bit view, which is WOW6432Node in HKLM; HKCU\Software is the same in both.
constexpr REGSAM ReadAccess{ KEY_READ | KEY_WOW64_32KEY };
constexpr DWORD MaxKeyNameLength{ 256 };

//////////////////////////////////////////////////////////////////////////
HKEY ToKey(ERegistryRoot root)
{
	return (root == ERegistryRoot::CurrentUser) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
}

//////////////////////////////////////////////////////////////////////////
std::wstring Widen(std::string_view ascii)
{
	return std::wstring{ ascii.begin(), ascii.end() };
}

//////////////////////////////////////////////////////////////////////////
std::string ToUtf8(std::wstring const& text)
{
	std::u8string const utf8{ std::filesystem::path{ text }.u8string() };

	return std::string{ utf8.begin(), utf8.end() };
}

//////////////////////////////////////////////////////////////////////////
// Nothing when the value is absent or not a string.
std::optional<std::wstring> ReadString(HKEY key, wchar_t const* pSubkey, wchar_t const* pValue)
{
	DWORD size{ 0 };
	std::optional<std::wstring> text{};

	if (RegGetValueW(key, pSubkey, pValue, RRF_RT_REG_SZ, nullptr, nullptr, &size) == ERROR_SUCCESS && size > 0)
	{
		std::wstring read{};

		read.resize(size / sizeof(wchar_t));

		if (RegGetValueW(key, pSubkey, pValue, RRF_RT_REG_SZ, nullptr, read.data(), &size) == ERROR_SUCCESS)
		{
			read.resize(std::wcslen(read.c_str()));
			text = std::move(read);
		}
	}

	return text;
}

//////////////////////////////////////////////////////////////////////////
void AddGogInstall(HKEY games, std::wstring const& subkey, std::vector<SGogInstall>& installs)
{
	std::string const id{ ToUtf8(subkey) };
	uint64_t number{ 0 };
	std::from_chars_result const parsed{ std::from_chars(id.data(), id.data() + id.size(), number) };
	std::optional<std::wstring> const folder{ ReadString(games, subkey.c_str(), L"path") };
	std::optional<std::wstring> const name{ ReadString(games, subkey.c_str(), L"gameName") };

	if (parsed.ec != std::errc{} || parsed.ptr != id.data() + id.size() || number == 0)
	{
		gLog.Warning("GOG's registry entry '{}' is skipped: it is not a game id", id);
	}
	else if (!folder.has_value() || folder->empty())
	{
		gLog.Warning("GOG's game {} is skipped: the registry names no folder for it", id);
	}
	else
	{
		installs.emplace_back(number, std::filesystem::path{ *folder }, ToUtf8(name.value_or(std::wstring{})));
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::filesystem::path ReadRegistryFolder(ERegistryRoot root, std::string_view key, std::string_view value)
{
	HKEY opened{ nullptr };
	std::filesystem::path folder{};

	if (RegOpenKeyExW(ToKey(root), Widen(key).c_str(), 0, ReadAccess, &opened) == ERROR_SUCCESS)
	{
		folder = ReadString(opened, nullptr, Widen(value).c_str()).value_or(std::wstring{});
		RegCloseKey(opened);
	}

	return folder;
}

//////////////////////////////////////////////////////////////////////////
// A missing key is no GOG game at all; a broken entry is skipped.
std::vector<SGogInstall> ReadGogInstalls(ERegistryRoot root, std::string_view key)
{
	HKEY opened{ nullptr };
	std::vector<SGogInstall> installs{};

	if (RegOpenKeyExW(ToKey(root), Widen(key).c_str(), 0, ReadAccess, &opened) == ERROR_SUCCESS)
	{
		LSTATUS status{ ERROR_SUCCESS };

		for (DWORD index{ 0 }; status == ERROR_SUCCESS; ++index)
		{
			std::wstring name{};
			DWORD length{ MaxKeyNameLength };

			name.resize(MaxKeyNameLength);
			status = RegEnumKeyExW(opened, index, name.data(), &length, nullptr, nullptr, nullptr, nullptr);

			if (status == ERROR_SUCCESS)
			{
				name.resize(length);
				AddGogInstall(opened, name, installs);
			}
		}

		RegCloseKey(opened);
	}

	return installs;
}
} // namespace Lkt::Launch
#endif // defined(TGE_PLATFORM_WINDOWS)
