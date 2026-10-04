#pragma once

#include <expected>
#include <string>

namespace Lkt::Net
{
// The error names the host and what went wrong: "raw.githubusercontent.com: answered 404 Not Found".
struct SFetchResult final
{
	std::string path;
	std::expected<std::string, std::string> body;
};
} // namespace Lkt::Net
