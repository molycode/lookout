#include "file_dialog.hpp"
#include "file_dialog_mailbox.hpp"
#include <tge/assert.hpp>
#include <SDL3/SDL.h>
#include <string_view>

namespace Lkt::Ui
{
namespace
{
// Static and trivially destructible: zenity calls back from a thread of its own, even after Lookout has shut down.
constinit SFileDialogMailbox gMailbox{};

//////////////////////////////////////////////////////////////////////////
// After shutdown neither the logger, the allocator nor SDL's event queue may be used.
void Finish(EFileDialogState state, std::string_view text)
{
	gMailbox.textLength = text.copy(gMailbox.text.data(), gMailbox.text.size());
	gMailbox.state.store(state, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
// zenity reports a cancel as one empty path.
void SDLCALL OnDialogFinished(void*, char const* const* pFileList, int)
{
	if (pFileList == nullptr)
	{
		std::string_view const error{ SDL_GetError() };

		Finish(EFileDialogState::Failed, error.empty() ? std::string_view{ "SDL gave no reason" } : error);
	}
	else if (pFileList[0] == nullptr || pFileList[0][0] == '\0')
	{
		Finish(EFileDialogState::Cancelled, {});
	}
	else
	{
		std::string_view const path{ pFileList[0] };

		if (path.size() <= gMailbox.text.size())
		{
			Finish(EFileDialogState::Picked, path);
		}
		else
		{
			Finish(EFileDialogState::Failed, "the chosen path is too long");
		}
	}
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// Open is stored first: SDL calls back before returning when it cannot show a dialog.
void OpenProgramDialog(SDL_Window* pWindow)
{
	TGE_ASSERT(gMailbox.state.load(std::memory_order_acquire) == EFileDialogState::Closed, "A program dialog opens over one whose answer was never taken");

	gMailbox.state.store(EFileDialogState::Open, std::memory_order_relaxed);
	SDL_ShowOpenFileDialog(&OnDialogFinished, nullptr, pWindow, nullptr, 0, nullptr, false);
}

//////////////////////////////////////////////////////////////////////////
void OpenFolderDialog(SDL_Window* pWindow, char const* pStartFolder)
{
	TGE_ASSERT(gMailbox.state.load(std::memory_order_acquire) == EFileDialogState::Closed, "A folder dialog opens over one whose answer was never taken");

	gMailbox.state.store(EFileDialogState::Open, std::memory_order_relaxed);
	SDL_ShowOpenFolderDialog(&OnDialogFinished, nullptr, pWindow, pStartFolder, false);
}

//////////////////////////////////////////////////////////////////////////
bool IsFileDialogPending()
{
	return gMailbox.state.load(std::memory_order_acquire) != EFileDialogState::Closed;
}

//////////////////////////////////////////////////////////////////////////
std::optional<SFileDialogResult> TakeFileDialogResult()
{
	std::optional<SFileDialogResult> result{};
	EFileDialogState const state{ gMailbox.state.load(std::memory_order_acquire) };

	if (state != EFileDialogState::Closed && state != EFileDialogState::Open)
	{
		result = SFileDialogResult{ state, std::string{ gMailbox.text.data(), gMailbox.textLength } };
		gMailbox.state.store(EFileDialogState::Closed, std::memory_order_relaxed);
	}

	return result;
}
} // namespace Lkt::Ui
