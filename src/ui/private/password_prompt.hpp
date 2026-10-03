#pragma once

#include <tge/non_copyable.hpp>
#include <cstdint>
#include <string>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Ui
{
// Asked each time and never stored.
class CPasswordPrompt final : private Tge::SNoCopyNoMove
{
public:

	CPasswordPrompt() = default;
	~CPasswordPrompt() = default;

	void Open(uint64_t key, std::string serverName, std::string launcherId);
	void Draw(Browser::CBrowser& browser, std::string& message);

private:

	std::string m_serverName;
	std::string m_launcherId;
	std::string m_password;
	std::string m_error;
	uint64_t m_key{ 0 };
	bool m_shouldOpen{ false };
	bool m_shouldFocus{ false };
};
} // namespace Ui
} // namespace Lkt
