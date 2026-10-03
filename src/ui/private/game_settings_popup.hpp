#pragma once

#include "file_dialog_purpose.hpp"
#include "install_edit.hpp"
#include "query/game.hpp"
#include <tge/non_copyable.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct SDL_Window;

namespace Lkt
{
namespace Browser
{
class CBrowser;
struct SInstallLauncher;
} // namespace Browser

namespace Config
{
struct SGameInstall;
} // namespace Config

namespace Ui
{
class CGameSettingsPopup final : private Tge::SNoCopyNoMove
{
public:

	CGameSettingsPopup() = default;
	~CGameSettingsPopup() = default;

	void Initialize(SDL_Window* pWindow);
	void Open(Query::EGame game);
	void Draw(Browser::CBrowser& browser, std::string& message);

private:

	void TakeDialogResult(Browser::CBrowser& browser, std::string& message);
	void UsePickedPath(Browser::CBrowser& browser, std::string_view path);
	void SyncEdits(Browser::CBrowser const& browser);
	void DrawFound(Browser::CBrowser const& browser) const;
	void DrawInstalls(Browser::CBrowser& browser);
	std::optional<uint32_t> DrawInstall(Browser::CBrowser& browser, SInstallEdit& edit, Config::SGameInstall const& install,
		Browser::SInstallLauncher const& launcher);
	void OpenDialog(EFileDialogPurpose purpose, uint32_t installId, std::string const& startFolder);

	SDL_Window* m_pWindow{ nullptr };
	std::vector<SInstallEdit> m_edits;
	Query::EGame m_game{ Query::EGame::Kingpin };
	Query::EGame m_dialogGame{ Query::EGame::Kingpin };
	uint32_t m_dialogInstallId{ 0 };
	EFileDialogPurpose m_dialogPurpose{ EFileDialogPurpose::AddFolder };
	bool m_shouldOpen{ false };
};
} // namespace Ui
} // namespace Lkt
