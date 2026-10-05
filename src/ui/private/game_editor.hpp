#pragma once

#include "discard_kind.hpp"
#include "discard_prompt.hpp"
#include "editor_outcome.hpp"
#include "editor_pending.hpp"
#include "games/field_problem.hpp"
#include "games/game_form_node.hpp"
#include "games/game_source.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Lkt::Ui
{
class CGameEditor final : private Tge::SNoCopyNoMove
{
public:

	CGameEditor() = default;
	~CGameEditor() = default;

	void Open(std::string_view key, std::filesystem::path const& userDir);
	void OpenNew();
	void AskToQuit();
	bool HasUnsavedChanges() const;
	void OnCatalogChanged(std::filesystem::path const& userDir);
	EEditorOutcome Draw(std::filesystem::path const& userDir, std::string& message);

private:

	void Load(std::string_view key, std::filesystem::path const& userDir);
	void LoadNew();
	void Prefill(std::string_view key, std::filesystem::path const& userDir);
	void Ask(EEditorPending pending, std::string_view key);
	void Check();
	void DrawNewGameHeader(std::filesystem::path const& userDir);
	bool DrawFooter(std::filesystem::path const& userDir, std::string& message, EEditorOutcome& outcome);
	bool DrawUnreadable(std::filesystem::path const& userDir);
	void DrawFileButtons(std::filesystem::path const& userDir);
	void ShowFolder(std::filesystem::path const& userDir);
	EEditorOutcome DrawDiscardPrompt(std::filesystem::path const& userDir);
	bool Save(std::filesystem::path const& userDir, std::string& message);
	std::string FindKeyProblem(std::filesystem::path const& userDir) const;

	std::string m_key;
	std::string m_name;
	std::string m_downloadedText;
	std::string m_savedAs;
	std::string m_unreadable;
	std::string m_keyProblem;
	std::string m_pendingKey;
	std::string m_prefilledFrom;
	std::string m_saveError;
	std::string m_note;
	std::string m_scriptSignature;
	Games::SGameFormNode m_form;
	Games::SGameFormNode m_openedForm;
	std::optional<Games::SGameFormNode> m_downloadedForm;
	std::optional<Games::SFieldProblem> m_problem;
	std::optional<Games::SFieldProblem> m_scriptProblem;
	CDiscardPrompt m_discardPrompt;
	float m_footerHeight{ 0.0f };
	EEditorPending m_pending{ EEditorPending::None };
	EDiscardKind m_discardKind{ EDiscardKind::Changes };
	Games::EGameSource m_source{ Games::EGameSource::None };
	bool m_isNew{ false };
	bool m_isOpen{ false };
	bool m_shouldFocus{ false };
	bool m_shouldAsk{ false };
};
} // namespace Lkt::Ui
