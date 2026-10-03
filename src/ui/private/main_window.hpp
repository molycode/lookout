#pragma once

#include "about_dialog.hpp"
#include "add_server_prompt.hpp"
#include "game_settings_popup.hpp"
#include "password_prompt.hpp"
#include "selection.hpp"
#include "toolbar.hpp"
#include "query/game.hpp"
#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

struct SDL_Window;

namespace Lkt
{
namespace Browser
{
class CBrowser;
struct SServerEntry;
} // namespace Browser

namespace Ui
{
struct SFrameIntents;
struct SServerAction;

// The views read the browser while drawing; what they ask for is applied afterwards, then the game settings popup and
// the prompts draw, acting on it directly.
class CMainWindow final : private Tge::SNoCopyNoMove
{
public:

	CMainWindow() = default;
	~CMainWindow() = default;

	void Initialize(SDL_Window* pWindow, uint32_t detailsWidth, SAboutInfo const& about);
	void Draw(Browser::CBrowser& browser);
	uint32_t GetDetailsWidth() const;

private:

	void DrawBody(Browser::CBrowser const& browser, SFrameIntents& intents);
	void DrawDetailsToggle();
	void Apply(Browser::CBrowser& browser, SFrameIntents const& intents);
	void SetGameListed(Browser::CBrowser& browser, Query::EGame game, bool isListed);
	void Handle(Browser::CBrowser& browser, SServerAction const& action);
	void Join(Browser::CBrowser& browser, Browser::SServerEntry const& entry, std::string_view launcherId);
	void CopyAddress(Query::SServerAddress const& address);
	uint64_t& GetSelectedKey(Query::EGame game);

	CToolbar m_toolbar;
	CPasswordPrompt m_passwordPrompt;
	CAddServerPrompt m_addServerPrompt;
	CAboutDialog m_aboutDialog;
	CGameSettingsPopup m_gameSettings;
	std::array<uint64_t, Query::NumGames> m_selectedKeys{ []()
	{
		std::array<uint64_t, Query::NumGames> keys{};

		keys.fill(NoSelection);

		return keys;
	}() };
	std::string m_message;
	float m_detailsEm{ 18.0f };
	bool m_shouldScrollToSelection{ false };
	bool m_isNarrow{ false };
	bool m_isDetailsShown{ false };
};
} // namespace Ui
} // namespace Lkt
