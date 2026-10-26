#include "user_agent.hpp"
#include <format>

namespace Lkt::Download
{
//////////////////////////////////////////////////////////////////////////
std::string MakeUserAgent(std::string_view version)
{
	return std::format("Lookout/{} (+https://github.com/molycode/lookout)", version);
}
} // namespace Lkt::Download
