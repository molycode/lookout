#pragma once

#include <string>
#include <vector>

namespace Lkt::Fixtures
{
// What the loopback HTTPS server answers one path with.
struct SHttpsReply final
{
	std::string status{ "200 OK" };
	std::string body{};
	// Beside Content-Length, which is sent unless one of these is a Transfer-Encoding.
	std::vector<std::string> headers{};
	// Answers nothing, keeping the connection open until the server stops.
	bool isSilent{ false };
	// Closes the connection after the reply without saying so, as a server dropping an idle connection does.
	bool dropsConnection{ false };
};
} // namespace Lkt::Fixtures
