#pragma once

#include <cstdint>
#include <string_view>

namespace Lkt::Launch
{
enum class EProgramError : uint8_t
{
	NotFound,
	NotRegularFile,
	NotExecutable
};

constexpr std::string_view ToString(EProgramError error)
{
	std::string_view text{ "unknown error" };

	switch (error)
	{
		case EProgramError::NotFound:
			text = "cannot be found";
			break;
		case EProgramError::NotRegularFile:
			text = "is not a regular file";
			break;
		case EProgramError::NotExecutable:
			text = "is not executable";
			break;
	}

	return text;
}
} // namespace Lkt::Launch
