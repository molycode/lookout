#pragma once

#include "file_dialog_result.hpp"
#include <optional>

struct SDL_Window;

namespace Lkt::Ui
{
// Main thread only, one dialog at a time.
void OpenProgramDialog(SDL_Window* pWindow);
void OpenFolderDialog(SDL_Window* pWindow, char const* pStartFolder);
// Until its answer is taken.
bool IsFileDialogPending();
// A finished dialog's answer, given once.
std::optional<SFileDialogResult> TakeFileDialogResult();
} // namespace Lkt::Ui
