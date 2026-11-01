#pragma once

#include "browser/browser.hpp"
#include "games/folder_watcher.hpp"
#include "ui/application.hpp"
#include <tge/non_copyable.hpp>

namespace Lkt
{
namespace Cli
{
class CSession;
} // namespace Cli

class CLookout final : private Tge::SNoCopyNoMove
{
public:

	CLookout() = default;
	~CLookout() = default;

	bool Run(Cli::CSession& session);

private:

	Games::CFolderWatcher m_watcher{};
	Browser::CBrowser m_browser{};
	Ui::CApplication m_application{};
};
} // namespace Lkt
