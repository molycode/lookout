#pragma once

#include "ui/about_info.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <string>

struct ImVec2;
struct SDL_Window;

namespace Lkt::Ui
{
class CAboutDialog final : private Tge::SNoCopyNoMove
{
public:

	CAboutDialog() = default;
	~CAboutDialog() = default;

	void Initialize(SDL_Window* pWindow, SAboutInfo const& about);
	void Open();
	void Draw();

private:

	void DrawAbout();
	void DrawSystem();
	void DrawLicences(ImVec2 const& pageSize);

	SDL_Window* m_pWindow{ nullptr };
	SAboutInfo m_about;
	std::string m_systemInfo;
	std::string m_result;
	size_t m_licenceIndex{ 0 };
	bool m_isResultError{ false };
	bool m_shouldOpen{ false };
};
} // namespace Lkt::Ui
