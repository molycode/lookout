#pragma once

#include "discard_kind.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <string>

namespace Lkt::Ui
{
// Asks before a game's files in the data folder are deleted.
class CDiscardPrompt final : private Tge::SNoCopyNoMove
{
public:

	CDiscardPrompt() = default;
	~CDiscardPrompt() = default;

	void Open(EDiscardKind kind, std::string key, std::string name, std::filesystem::path const& userDir);
	// True once the deletion was tried, so the games are read again whatever it left.
	bool Draw(std::filesystem::path const& userDir, std::string& message);

private:

	void Discard(std::filesystem::path const& userDir, std::string& message) const;

	std::string m_key;
	std::string m_name;
	std::string m_question;
	EDiscardKind m_kind{ EDiscardKind::Changes };
	bool m_shouldOpen{ false };
};
} // namespace Lkt::Ui
