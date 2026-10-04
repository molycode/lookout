#pragma once

#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
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

	void Open(Query::SServerAddress const& joinAddress, std::string serverName, std::string launcherId);
	void Draw(Browser::CBrowser& browser, std::string& message);

private:

	std::string m_serverName;
	std::string m_launcherId;
	std::string m_password;
	std::string m_error;
	Query::SServerAddress m_joinAddress;
	bool m_shouldOpen{ false };
	bool m_shouldFocus{ false };
};
} // namespace Ui
} // namespace Lkt
