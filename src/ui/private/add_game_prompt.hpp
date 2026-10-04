#pragma once

#include "new_game.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <optional>
#include <string>

namespace Lkt::Ui
{
class CAddGamePrompt final : private Tge::SNoCopyNoMove
{
public:

	CAddGamePrompt() = default;
	~CAddGamePrompt() = default;

	void Open();
	// The new game's key and the text it starts from, once the user goes on.
	std::optional<SNewGame> Draw(std::filesystem::path const& userDir);

private:

	void DrawStartFrom();
	std::optional<SNewGame> Submit(std::filesystem::path const& userDir);

	std::string m_key;
	std::string m_startFrom;
	std::string m_error;
	bool m_shouldOpen{ false };
	bool m_shouldFocus{ false };
};
} // namespace Lkt::Ui
