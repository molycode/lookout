#include "ui/show_error.hpp"
#include "loggers.hpp"
#include <SDL3/SDL.h>
#include <string>

namespace Lkt::Ui
{
//////////////////////////////////////////////////////////////////////////
// SDL shows a message box without being initialized, so this works when the window could not start.
void ShowError(std::string_view text)
{
	std::string const message{ text };

	if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lookout", message.c_str(), nullptr))
	{
		gLog.Error("Cannot show the message '{}': {}", message, SDL_GetError());
	}
}
} // namespace Lkt::Ui
