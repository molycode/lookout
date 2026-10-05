#include "game_editor.hpp"
#include "format_to.hpp"
#include "game_form_view.hpp"
#include "icons.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "games/check_game.hpp"
#include "games/editable_game.hpp"
#include "games/game_files.hpp"
#include "games/game_form.hpp"
#include "games/game_source.hpp"
#include "launch/file_url.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <SDL3/SDL.h>
#include <array>
#include <cfloat>
#include <expected>
#include <format>
#include <span>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr char const* WindowId{ "###game-editor" };
constexpr char const* DiscardPopupId{ "Discard changes###editor-discard" };
constexpr char const* NothingLabel{ "Nothing" };
constexpr float WidthEm{ 50.0f };
constexpr float HeightEm{ 40.0f };
constexpr float HeaderLabelEm{ 13.0f };
constexpr float PromptWidthEm{ 22.0f };
constexpr std::string_view ProtocolField{ "protocol" };
constexpr std::string_view OptionsField{ "protocolOptions" };

//////////////////////////////////////////////////////////////////////////
// Typed capitals become small letters and anything a key cannot hold is dropped, so most keys are valid as typed.
int FilterKeyCharacter(ImGuiInputTextCallbackData* pData)
{
	ImWchar const c{ pData->EventChar };
	bool const isUpper{ c >= 'A' && c <= 'Z' };

	if (isUpper)
	{
		pData->EventChar = static_cast<ImWchar>(c - 'A' + 'a');
	}

	bool const isKept{ isUpper || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' };

	return isKept ? 0 : 1;
}

//////////////////////////////////////////////////////////////////////////
// The protocol and its options alone decide what the protocol's conversations make of a description.
std::string MakeScriptSignature(Games::SGameFormNode const& form)
{
	Games::SGameFormNode const* const pProtocol{ Games::FindFormField(form, ProtocolField) };
	Games::SGameFormNode const* const pOptions{ Games::FindFormField(form, OptionsField) };
	std::string signature{ (pProtocol != nullptr) ? pProtocol->text : std::string{} };

	if (pOptions != nullptr)
	{
		for (Games::SGameFormNode const& option : pOptions->children)
		{
			signature += std::format("\n{}={}", option.name, option.text);
		}
	}

	return signature;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CGameEditor::Open(std::string_view key, std::filesystem::path const& userDir)
{
	if (m_isOpen && !m_isNew && key == m_key)
	{
		m_shouldFocus = true;
	}
	else if (HasUnsavedChanges())
	{
		Ask(EEditorPending::Open, key);
		m_shouldFocus = true;
	}
	else
	{
		Load(key, userDir);
		m_shouldFocus = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::OpenNew()
{
	if (m_isOpen && m_isNew)
	{
		m_shouldFocus = true;
	}
	else if (HasUnsavedChanges())
	{
		Ask(EEditorPending::New, {});
		m_shouldFocus = true;
	}
	else
	{
		LoadNew();
		m_shouldFocus = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::AskToQuit()
{
	Ask(EEditorPending::Quit, {});
	m_shouldFocus = true;
}

//////////////////////////////////////////////////////////////////////////
bool CGameEditor::HasUnsavedChanges() const
{
	return m_isOpen && m_unreadable.empty() && (m_form != m_openedForm || (m_isNew && !m_key.empty()));
}

//////////////////////////////////////////////////////////////////////////
// Unedited, the editor shows the files as they now are; edited, it keeps the edits, which a save puts on top of them.
void CGameEditor::OnCatalogChanged(std::filesystem::path const& userDir)
{
	if (m_isOpen && !m_isNew && HasUnsavedChanges())
	{
		m_note = "This game's files changed while you edited it. Saving keeps your changes on top of them.";
	}
	else if (m_isOpen && !m_isNew)
	{
		Load(m_key, userDir);
	}
}

//////////////////////////////////////////////////////////////////////////
EEditorOutcome CGameEditor::Draw(std::filesystem::path const& userDir, std::string& message)
{
	EEditorOutcome outcome{ EEditorOutcome::None };

	if (m_isOpen)
	{
		float const em{ ImGui::GetFontSize() };
		std::array<char, 160> title{};
		bool isOpen{ true };

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
		ImGui::SetNextWindowSize(ImVec2{ WidthEm * em, HeightEm * em }, ImGuiCond_Appearing);

		if (m_shouldFocus)
		{
			ImGui::SetNextWindowFocus();
			m_shouldFocus = false;
		}

		std::string_view const name{ m_isNew ? FormatTo(title, "New game{}", WindowId) : FormatTo(title, "{}{}", m_name, WindowId) };

		if (ImGui::Begin(name.data(), &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
		{
			// Routed to the focused editor: a field being typed in takes Escape first, to undo its typing.
			bool const wantsEscape{ ImGui::Shortcut(ImGuiKey_Escape) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) };
			bool isCloseClicked{ false };

			if (!m_unreadable.empty())
			{
				isCloseClicked = DrawUnreadable(userDir);
			}
			else
			{
				if (m_isNew)
				{
					DrawNewGameHeader(userDir);
				}

				ImGui::BeginChild("##form", ImVec2{ 0.0f, -m_footerHeight }, ImGuiChildFlags_NavFlattened);

				if (DrawGameForm(m_form, m_downloadedForm.has_value() ? &*m_downloadedForm : nullptr, m_problem.has_value() ? &*m_problem : nullptr))
				{
					Check();
				}

				ImGui::EndChild();

				float const footerStart{ ImGui::GetCursorPosY() };

				isCloseClicked = DrawFooter(userDir, message, outcome);
				m_footerHeight = ImGui::GetCursorPosY() - footerStart;
			}

			if (!isOpen || wantsEscape || isCloseClicked)
			{
				if (HasUnsavedChanges())
				{
					Ask(EEditorPending::Close, {});
				}
				else
				{
					m_isOpen = false;
				}
			}

			EEditorOutcome const prompted{ DrawDiscardPrompt(userDir) };

			outcome = (prompted != EEditorOutcome::None) ? prompted : outcome;

			if (m_discardPrompt.Draw(userDir, message))
			{
				outcome = EEditorOutcome::FilesChanged;

				if (m_discardKind == EDiscardKind::Changes)
				{
					Load(m_key, userDir);
				}
				else
				{
					m_isOpen = false;
				}
			}
		}

		ImGui::End();
	}

	return outcome;
}

//////////////////////////////////////////////////////////////////////////
// The downloaded text is kept as opened, so a save makes its patch against what the user saw.
void CGameEditor::Load(std::string_view key, std::filesystem::path const& userDir)
{
	Games::SEditableGame const opened{ Games::ReadGameText(userDir, key) };
	Query::SGameDefinition const* const pGame{ Query::FindGame(key) };
	std::expected<Games::SGameFormNode, std::string> form{ opened.text.empty() ? Games::MakeGameForm() : Games::ReadGameForm(opened.text) };
	std::expected<Games::SGameFormNode, std::string> downloaded{ std::unexpect };
	std::string const file{ (userDir / "games" / key / "game.json").string() };

	if (!opened.downloaded.empty())
	{
		downloaded = Games::ReadGameForm(opened.downloaded);
	}

	m_key = key;
	m_name = (pGame != nullptr) ? pGame->name : m_key;
	m_source = Games::FindGameSource(userDir, key);
	m_downloadedText = opened.downloaded;
	m_savedAs = opened.downloaded.empty() ? std::format("Saved as {}", file) : std::format("Saved as your changes to the download, in {}", file);
	m_unreadable = !opened.problem.empty() ? opened.problem : (form.has_value() ? std::string{} : std::format("games/{}/game.json: {}", key, form.error()));
	m_form = form.has_value() ? std::move(*form) : Games::SGameFormNode{};
	m_openedForm = m_form;
	m_downloadedForm = downloaded.has_value() ? std::optional<Games::SGameFormNode>{ std::move(*downloaded) } : std::nullopt;
	m_keyProblem.clear();
	m_prefilledFrom.clear();
	m_saveError.clear();
	m_note.clear();
	m_scriptSignature.clear();
	m_scriptProblem.reset();
	m_isNew = false;
	m_isOpen = true;
	Check();
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::LoadNew()
{
	m_key.clear();
	m_name.clear();
	m_source = Games::EGameSource::None;
	m_downloadedText.clear();
	m_savedAs.clear();
	m_unreadable.clear();
	m_form = Games::MakeGameForm();
	m_openedForm = m_form;
	m_downloadedForm.reset();
	m_keyProblem.clear();
	m_prefilledFrom.clear();
	m_saveError.clear();
	m_note.clear();
	m_scriptSignature.clear();
	m_scriptProblem.reset();
	m_isNew = true;
	m_isOpen = true;
	Check();
}

//////////////////////////////////////////////////////////////////////////
// Another game's notes explain its own choices, so they do not come along.
void CGameEditor::Prefill(std::string_view key, std::filesystem::path const& userDir)
{
	std::expected<Games::SGameFormNode, std::string> form{ Games::ReadGameForm(Games::ReadGameText(userDir, key).text) };

	if (form.has_value())
	{
		m_form = std::move(*form);
		Games::ClearFormNotes(m_form);
		m_prefilledFrom = key;
		Check();
	}
	else
	{
		m_note = std::format("Cannot prefill from {}: {}", key, form.error());
	}
}

//////////////////////////////////////////////////////////////////////////
// Focusing the editor would close the prompt over it, so only a request from outside, made before the editor draws,
// brings it to the front.
void CGameEditor::Ask(EEditorPending pending, std::string_view key)
{
	m_pending = pending;
	m_pendingKey = key;
	m_shouldAsk = true;
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::Check()
{
	std::span<Query::SProtocolDefinition const> const protocols{ Query::GetProtocolCatalog() };
	std::string const text{ Games::WriteGameForm(m_form) };
	std::expected<void, Games::SFieldProblem> const fields{ Games::CheckGameFields(text, protocols) };
	std::string const signature{ MakeScriptSignature(m_form) };

	if (fields.has_value() && signature != m_scriptSignature)
	{
		std::expected<void, Games::SFieldProblem> const checked{ Games::CheckGameText(text, protocols) };

		m_scriptProblem = checked.has_value() ? std::nullopt : std::optional<Games::SFieldProblem>{ checked.error() };
		m_scriptSignature = signature;
	}

	m_problem = fields.has_value() ? m_scriptProblem : std::optional<Games::SFieldProblem>{ fields.error() };
	m_saveError.clear();
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::DrawNewGameHeader(std::filesystem::path const& userDir)
{
	float const labelX{ ImGui::GetFontSize() * HeaderLabelEm };
	Query::SGameDefinition const* const pFrom{ Query::FindGame(m_prefilledFrom) };

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Key");
	ImGui::SameLine(labelX);

	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetKeyboardFocusHere();
	}

	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::InputTextWithHint("##key", "my-game: small letters, digits, '-' and '_'", &m_key, ImGuiInputTextFlags_CallbackCharFilter, FilterKeyCharacter))
	{
		m_keyProblem = FindKeyProblem(userDir);
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Prefill from");
	ImGui::SameLine(labelX);
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::BeginCombo("##prefill", (pFrom != nullptr) ? pFrom->name.c_str() : NothingLabel))
	{
		for (Query::SGameDefinition const& game : Query::GetGameCatalog())
		{
			ImGui::PushID(game.key.c_str());

			bool const isChosen{ ImGui::Selectable(game.name.c_str(), &game == pFrom) };

			if (isChosen && m_form != m_openedForm)
			{
				Ask(EEditorPending::Prefill, game.key);
			}
			else if (isChosen)
			{
				Prefill(game.key, userDir);
			}

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	ImGui::Separator();
}

//////////////////////////////////////////////////////////////////////////
bool CGameEditor::DrawFooter(std::filesystem::path const& userDir, std::string& message, EEditorOutcome& outcome)
{
	SThemeColors const& colors{ GetThemeColors() };
	bool const isEdited{ HasUnsavedChanges() };

	ImGui::PushTextWrapPos(0.0f);

	if (!m_note.empty())
	{
		ImGui::TextColored(colors.amber, "%s", m_note.c_str());
	}

	if (!m_saveError.empty())
	{
		ImGui::TextColored(colors.error, "%s", m_saveError.c_str());
	}
	else if (!m_keyProblem.empty())
	{
		ImGui::TextColored(colors.error, "%s", m_keyProblem.c_str());
	}
	else if (m_problem.has_value())
	{
		ImGui::TextColored(colors.error, "%s", DescribeFieldProblem(*m_problem).c_str());
	}
	else
	{
		ImGui::TextColored(colors.pingGood, "%s", LKT_ICON_CHECK " Lookout can use this description");
	}

	ImGui::PopTextWrapPos();
	ImGui::BeginDisabled(m_problem.has_value() || !m_keyProblem.empty() || (m_isNew && m_key.empty()) || !(isEdited || m_isNew));

	bool const isSaveClicked{ ImGui::Button("Save") };

	ImGui::EndDisabled();
	ImGui::SameLine();

	bool const isCloseClicked{ ImGui::Button("Close") };

	DrawFileButtons(userDir);
	ImGui::SameLine(0.0f, ImGui::GetFontSize());
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%s", m_savedAs.c_str());

	if (isSaveClicked && Save(userDir, message))
	{
		outcome = EEditorOutcome::FilesChanged;
	}

	return isCloseClicked;
}

//////////////////////////////////////////////////////////////////////////
bool CGameEditor::DrawUnreadable(std::filesystem::path const& userDir)
{
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextColored(GetThemeColors().error, "Lookout cannot show this description: %s", m_unreadable.c_str());
	ImGui::Spacing();
	ImGui::TextUnformatted("Fix the file in a text editor, and the editor shows it once it can be read, or let it go.");

	if (!m_note.empty())
	{
		ImGui::TextColored(GetThemeColors().amber, "%s", m_note.c_str());
	}

	ImGui::PopTextWrapPos();
	ImGui::Spacing();

	bool const isCloseClicked{ ImGui::Button("Close") };

	DrawFileButtons(userDir);
	ImGui::SameLine();

	if (ImGui::Button("Show folder"))
	{
		ShowFolder(userDir);
	}

	return isCloseClicked;
}

//////////////////////////////////////////////////////////////////////////
// A downloaded game's changes can be reverted, a game of the user's own removed; each asks first.
void CGameEditor::DrawFileButtons(std::filesystem::path const& userDir)
{
	bool const canRevert{ m_source == Games::EGameSource::Patched };
	bool const canRemove{ m_source == Games::EGameSource::User };

	if (canRevert || canRemove)
	{
		ImGui::SameLine(0.0f, ImGui::GetFontSize());

		if (ImGui::Button(canRevert ? "Revert to downloaded…" : "Remove…"))
		{
			m_discardKind = canRevert ? EDiscardKind::Changes : EDiscardKind::Game;
			m_discardPrompt.Open(m_discardKind, m_key, m_name, userDir);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameEditor::ShowFolder(std::filesystem::path const& userDir)
{
	std::string const url{ Launch::ToFileUrl((userDir / "games" / m_key).string()) };

	if (!SDL_OpenURL(url.c_str()))
	{
		gLog.Warning("Cannot show the folder of {}: {}", m_key, SDL_GetError());
		m_note = std::format("Cannot show the folder: {}", SDL_GetError());
	}
}

//////////////////////////////////////////////////////////////////////////
EEditorOutcome CGameEditor::DrawDiscardPrompt(std::filesystem::path const& userDir)
{
	EEditorOutcome outcome{ EEditorOutcome::None };

	if (m_shouldAsk)
	{
		ImGui::OpenPopup(DiscardPopupId);
		m_shouldAsk = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(DiscardPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		std::array<char, 160> question{};
		std::string_view const text{ m_isNew ? FormatTo(question, "Discard the new game?") : FormatTo(question, "Discard your changes to {}?", m_name) };

		ImGui::PushTextWrapPos(ImGui::GetFontSize() * PromptWidthEm);
		ImGui::TextUnformatted(text.data(), text.data() + text.size());
		ImGui::PopTextWrapPos();
		ImGui::Spacing();

		bool const isDiscarded{ ImGui::Button("Discard") };

		ImGui::SameLine();

		// The Escape that asked is still down while the prompt appears; it must not also answer it.
		bool const isKept{ ImGui::Button("Keep editing") || (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsWindowAppearing()) };

		if (isDiscarded)
		{
			switch (m_pending)
			{
				case EEditorPending::Close:
					m_isOpen = false;
					break;
				case EEditorPending::Open:
					Load(m_pendingKey, userDir);
					break;
				case EEditorPending::New:
					LoadNew();
					break;
				case EEditorPending::Prefill:
					Prefill(m_pendingKey, userDir);
					break;
				case EEditorPending::Quit:
					outcome = EEditorOutcome::Quit;
					break;
				case EEditorPending::None:
					break;
			}
		}

		if (isDiscarded || isKept)
		{
			m_pending = EEditorPending::None;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return outcome;
}

//////////////////////////////////////////////////////////////////////////
// A new game's key is checked again here: a download may have brought the same key while the editor was open.
bool CGameEditor::Save(std::filesystem::path const& userDir, std::string& message)
{
	m_keyProblem = m_isNew ? FindKeyProblem(userDir) : std::string{};

	std::string const key{ m_key };
	std::expected<void, std::string> const saved{ m_keyProblem.empty() ? Games::SaveGame(userDir, key, Games::WriteGameForm(m_form), m_downloadedText)
		: std::expected<void, std::string>{ std::unexpected{ m_keyProblem } } };

	if (saved.has_value())
	{
		message = m_isNew ? std::format("Added the game {}", key) : std::format("Saved the description of {}", m_name);
		Load(key, userDir);
	}
	else if (m_keyProblem.empty())
	{
		gLog.Warning("Cannot save the description of {}: {}", m_name, saved.error());
		m_saveError = std::format("Not saved: {}", saved.error());
	}

	return saved.has_value();
}

//////////////////////////////////////////////////////////////////////////
std::string CGameEditor::FindKeyProblem(std::filesystem::path const& userDir) const
{
	Query::SGameDefinition const* const pSameKey{ Query::FindGame(m_key) };
	Games::EGameSource const source{ Games::IsValidKey(m_key) ? Games::FindGameSource(userDir, m_key) : Games::EGameSource::None };
	std::string problem{};

	if (!Games::IsValidKey(m_key))
	{
		problem = "A key starts with a small letter or a digit.";
	}
	else if (pSameKey != nullptr)
	{
		problem = std::format("{} already has this key.", pSameKey->name);
	}
	else if (source == Games::EGameSource::Downloaded || source == Games::EGameSource::Patched)
	{
		problem = "A downloaded game has this key.";
	}
	else if (source == Games::EGameSource::User)
	{
		problem = std::format("{} already exists.", (userDir / "games" / m_key).string());
	}

	return problem;
}
} // namespace Lkt::Ui
