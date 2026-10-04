#pragma once

#include "new_game.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <string>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Ui
{
// A game description as JSON text, checked as it is typed, beside a reference of what it may hold.
class CGameEditor final : private Tge::SNoCopyNoMove
{
public:

	CGameEditor() = default;
	~CGameEditor() = default;

	void Open(Query::SGameDefinition const& game, std::filesystem::path const& userDir);
	void OpenNew(SNewGame game, std::filesystem::path const& userDir);
	// True once the description was saved.
	bool Draw(std::filesystem::path const& userDir, std::string& message);

private:

	void Opened(std::filesystem::path const& userDir);
	void Check();
	void DrawStatus(float width) const;
	bool Save(std::filesystem::path const& userDir, std::string& message);

	std::string m_key;
	std::string m_name;
	std::string m_text;
	std::string m_openedText;
	std::string m_note;
	std::string m_problem;
	std::string m_saveError;
	std::string m_savedAs;
	float m_footerHeight{ 0.0f };
	bool m_isBuiltin{ false };
	bool m_isNew{ false };
	bool m_shouldOpen{ false };
};
} // namespace Ui
} // namespace Lkt
