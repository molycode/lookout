#pragma once

#include "file_dialog_state.hpp"
#include <array>
#include <atomic>
#include <cstddef>

namespace Lkt::Ui
{
// The state's release store publishes the text.
struct SFileDialogMailbox final
{
	std::atomic<EFileDialogState> state{ EFileDialogState::Closed };
	std::array<char, 4096> text{};
	size_t textLength{ 0 };
};
} // namespace Lkt::Ui
