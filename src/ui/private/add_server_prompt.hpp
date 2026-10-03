#pragma once

#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <optional>
#include <string>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Ui
{
class CAddServerPrompt final : private Tge::SNoCopyNoMove
{
public:

	CAddServerPrompt() = default;
	~CAddServerPrompt() = default;

	void Open();
	std::optional<Query::SServerAddress> Draw(Browser::CBrowser& browser);

private:

	std::string m_text;
	std::string m_error;
	bool m_shouldOpen{ false };
	bool m_shouldFocus{ false };
};
} // namespace Ui
} // namespace Lkt
