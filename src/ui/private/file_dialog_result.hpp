#pragma once

#include "file_dialog_state.hpp"
#include <string>

namespace Lkt::Ui
{
struct SFileDialogResult final
{
	EFileDialogState state{ EFileDialogState::Closed };
	std::string text;
};
} // namespace Lkt::Ui
