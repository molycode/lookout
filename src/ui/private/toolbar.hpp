#pragma once

#include "query/game.hpp"
#include <tge/non_copyable.hpp>
#include <string>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Config
{
struct SServerFilter;
} // namespace Config

namespace Ui
{
struct SFrameIntents;

class CToolbar final : private Tge::SNoCopyNoMove
{
public:

	CToolbar() = default;
	~CToolbar() = default;

	void Draw(Browser::CBrowser const& browser, bool canEditGames, SFrameIntents& intents);
	void OnCatalogChanged();

private:


	std::string m_search;
	Query::EGame m_game{ Query::NoGame };
	bool m_hasGame{ false };
	bool m_shouldFocusSearch{ false };
};
} // namespace Ui
} // namespace Lkt
