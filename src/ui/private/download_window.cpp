#include "download_window.hpp"
#include "format_to.hpp"
#include "icon_textures.hpp"
#include "icons.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "download/lookout_games.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr float WidthEm{ 32.0f };
constexpr float HeightEm{ 28.0f };
constexpr float PromptWidthEm{ 22.0f };
constexpr char const* RemoveAllPopupId{ "Remove all games###removeAll" };
constexpr std::array<Download::EOfferState, 2> Downloadable{ Download::EOfferState::NotInstalled, Download::EOfferState::UpdateAvailable };
constexpr std::array<Download::EOfferState, 3> Removable{ Download::EOfferState::Installed, Download::EOfferState::UpdateAvailable,
	Download::EOfferState::Withdrawn };

//////////////////////////////////////////////////////////////////////////
std::string_view Describe(Download::EOfferState state)
{
	std::string_view text{};

	switch (state)
	{
		case Download::EOfferState::NotInstalled:
			text = "Not installed";
			break;
		case Download::EOfferState::Installed:
			text = "Installed";
			break;
		case Download::EOfferState::UpdateAvailable:
			text = "Update available";
			break;
		case Download::EOfferState::NeedsNewerLookout:
			text = "Needs a newer Lookout";
			break;
		case Download::EOfferState::Withdrawn:
			text = "No longer offered";
			break;
	}

	return text;
}

//////////////////////////////////////////////////////////////////////////
bool Button(char const* pLabel, bool isEnabled, char const* pTooltip)
{
	ImGui::BeginDisabled(!isEnabled);

	bool const isPressed{ ImGui::Button(pLabel) };

	ImGui::EndDisabled();
	ImGui::SetItemTooltip("%s", pTooltip);

	return isPressed;
}

//////////////////////////////////////////////////////////////////////////
bool GameButton(char const* id, std::string_view glyph, std::string_view tooltip)
{
	bool const isPressed{ IconButton(id, glyph) };

	ImGui::SetItemTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());

	return isPressed;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::Initialize(SDL_Renderer* pRenderer, std::filesystem::path const& userDir, std::string_view version, std::function<void()> wake)
{
	m_pRenderer = pRenderer;
	m_isReady = !userDir.empty()
		&& m_downloads.Initialize(userDir, Download::GetLookoutGamesSource(version), Download::EIndexIcons::Fetch, std::move(wake));

	if (!userDir.empty() && !m_isReady)
	{
		gLog.Warning("Games cannot be downloaded: the download could not be set up");
	}
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::Terminate()
{
	m_downloads.Terminate();

	for (auto& [hash, levels] : m_icons)
	{
		DestroyIconLevels(levels);
	}

	m_icons.clear();
	m_isReady = false;
}

//////////////////////////////////////////////////////////////////////////
// The list is read anew on every opening, so it shows what lookout-games offers now.
void CDownloadWindow::Open()
{
	if (m_isReady)
	{
		m_isOpen = true;
		m_shouldFocus = true;
		m_downloads.ReadIndex();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CDownloadWindow::Update()
{
	return m_isReady && m_downloads.Update();
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::Draw()
{
	if (m_isOpen)
	{
		float const em{ ImGui::GetFontSize() };

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
		ImGui::SetNextWindowSize(ImVec2{ WidthEm * em, HeightEm * em }, ImGuiCond_Appearing);

		if (m_shouldFocus)
		{
			ImGui::SetNextWindowFocus();
			m_shouldFocus = false;
		}

		if (ImGui::Begin("Download games###downloads", &m_isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
		{
			DrawStatus();
			DrawGames();
			DrawButtons();
		}

		ImGui::End();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CDownloadWindow::CanOpen() const
{
	return m_isReady;
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::DrawStatus() const
{
	std::array<char, 64> buffer{};

	ImGui::TextDisabled("From github.com/molycode/lookout-games");

	if (m_downloads.GetPhase() == Download::EDownloadPhase::ReadingIndex)
	{
		ImGui::TextUnformatted("Reading the list of games…");
	}
	else if (m_downloads.GetPhase() == Download::EDownloadPhase::Downloading)
	{
		std::string_view const progress{ FormatTo(buffer, "Downloading: {} of {} files…", m_downloads.GetNumFetched(), m_downloads.GetNumToFetch()) };

		ImGui::TextUnformatted(progress.data(), progress.data() + progress.size());
	}

	ImGui::PushTextWrapPos(0.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().error);

	for (std::string const& problem : m_downloads.GetProblems())
	{
		ImGui::TextUnformatted(problem.data(), problem.data() + problem.size());
	}

	ImGui::PopStyleColor();
	ImGui::PopTextWrapPos();
}

//////////////////////////////////////////////////////////////////////////
// A click is acted on after the list is drawn: a removal rebuilds the offers being drawn.
void CDownloadWindow::DrawGames()
{
	std::vector<std::string> toDownload{};
	std::vector<std::string> toRemove{};

	if (ImGui::BeginChild("##games", ImVec2{ 0.0f, -ImGui::GetFrameHeightWithSpacing() }))
	{
		for (Download::SGameOffer const& offer : m_downloads.GetOffers())
		{
			DrawGame(offer, toDownload, toRemove);
		}
	}

	ImGui::EndChild();

	if (!toDownload.empty())
	{
		m_downloads.Download(toDownload);
	}

	if (!toRemove.empty())
	{
		m_downloads.Remove(toRemove);
	}
}

//////////////////////////////////////////////////////////////////////////
// A group, so the layout carries on below the card.
void CDownloadWindow::DrawGame(Download::SGameOffer const& offer, std::vector<std::string>& toDownload, std::vector<std::string>& toRemove)
{
	SThemeColors const& colors{ GetThemeColors() };
	ImGuiStyle const& style{ ImGui::GetStyle() };
	Download::EOfferState const state{ offer.state };
	bool const isDownloading{ std::ranges::contains(m_downloads.GetDownloadKeys(), offer.key) };
	bool const canGet{ state == Download::EOfferState::NotInstalled };
	bool const canUpdate{ state == Download::EOfferState::UpdateAvailable };
	bool const canRemove{ std::ranges::contains(Removable, state) };
	float const numButtons{ static_cast<float>(static_cast<int>(canGet) + static_cast<int>(canUpdate) + static_cast<int>(canRemove)) };
	float const lineHeight{ ImGui::GetTextLineHeight() };
	ImVec2 const padding{ style.FramePadding };
	ImVec2 const start{ ImGui::GetCursorScreenPos() };
	float const iconSize{ ImGui::GetTextLineHeightWithSpacing() + lineHeight };
	ImVec2 const size{ ImGui::GetContentRegionAvail().x, padding.y * 2.0f + iconSize };
	ImVec2 const icon{ start.x + padding.x, start.y + padding.y };
	ImVec2 const text{ icon.x + iconSize + style.ItemInnerSpacing.x, icon.y };
	float const buttonsX{ start.x + size.x - padding.x - numButtons * lineHeight - std::max(numButtons - 1.0f, 0.0f) * style.ItemInnerSpacing.x };
	std::string_view const stateText{ isDownloading ? std::string_view{ "Downloading…" } : Describe(state) };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };
	std::array<char, 128> buffer{};

	ImGui::PushID(offer.key.c_str());
	ImGui::BeginGroup();
	ImGui::Dummy(size);
	pDrawList->AddRectFilled(start, ImVec2{ start.x + size.x, start.y + size.y }, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetFontSize() * CardRoundingEm);
	DrawIcon(pDrawList, FindIcon(offer), icon, iconSize);
	DrawEllipsised(offer.name, text, buttonsX - style.ItemInnerSpacing.x, colors.text);
	DrawEllipsised(stateText, ImVec2{ text.x, text.y + ImGui::GetTextLineHeightWithSpacing() }, start.x + size.x - padding.x,
		(isDownloading || canUpdate) ? colors.amber : colors.textDisabled);
	ImGui::SetCursorScreenPos(ImVec2{ buttonsX, text.y });
	ImGui::BeginDisabled(m_downloads.GetPhase() != Download::EDownloadPhase::Idle);

	if (canGet && GameButton("##get", LKT_ICON_DOWNLOAD, FormatTo(buffer, "Download {}", offer.name)))
	{
		toDownload.emplace_back(offer.key);
	}

	if (canUpdate)
	{
		if (GameButton("##update", LKT_ICON_CIRCLE_UP, FormatTo(buffer, "Download the update of {}", offer.name)))
		{
			toDownload.emplace_back(offer.key);
		}

		ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
	}

	if (canRemove && GameButton("##remove", LKT_ICON_TRASH, FormatTo(buffer, "Remove {}; your own changes to it stay", offer.name)))
	{
		toRemove.emplace_back(offer.key);
	}

	ImGui::EndDisabled();
	ImGui::EndGroup();
	ImGui::PopID();
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::DrawButtons()
{
	bool const isIdle{ m_downloads.GetPhase() == Download::EDownloadPhase::Idle };

	if (Button("Download all", CanDownload() && HasAny(Downloadable), "Every game not installed or with an update"))
	{
		m_downloads.Download(Collect(Downloadable));
	}

	ImGui::SameLine();

	if (Button("Remove all", isIdle && HasAny(Removable), "Every downloaded game; your own changes to them stay"))
	{
		ImGui::OpenPopup(RemoveAllPopupId);
	}

	DrawRemoveAllPrompt();
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::DrawRemoveAllPrompt()
{
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(RemoveAllPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * PromptWidthEm);
		ImGui::TextUnformatted("Remove every downloaded game? Your own changes to them stay, and each can be downloaded again.");
		ImGui::PopTextWrapPos();
		ImGui::Spacing();

		bool const isConfirmed{ ImGui::Button("Remove") };

		ImGui::SameLine();

		bool const shouldClose{ isConfirmed || ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) };

		if (isConfirmed)
		{
			m_downloads.Remove(Collect(Removable));
		}

		if (shouldClose)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

//////////////////////////////////////////////////////////////////////////
// Made into textures the first time it is drawn; one that cannot be is kept empty, so it is not tried every frame.
std::span<SIconLevel const> CDownloadWindow::FindIcon(Download::SGameOffer const& offer)
{
	auto icon{ m_icons.find(offer.iconSha256) };

	if (icon == m_icons.end() && !offer.iconSha256.empty())
	{
		std::string_view const png{ m_downloads.GetIcon(offer.iconSha256) };

		if (!png.empty())
		{
			icon = m_icons.emplace(offer.iconSha256, LoadIconLevels(m_pRenderer, std::as_bytes(std::span{ png }))).first;

			if (icon->second.empty())
			{
				gLog.Warning("Cannot load the icon of {}, so a stand-in shows instead: {}", offer.name, SDL_GetError());
			}
		}
	}

	return (icon != m_icons.end()) ? std::span<SIconLevel const>{ icon->second } : std::span<SIconLevel const>{};
}

//////////////////////////////////////////////////////////////////////////
bool CDownloadWindow::CanDownload() const
{
	return m_downloads.GetPhase() == Download::EDownloadPhase::Idle && m_downloads.HasIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CDownloadWindow::HasAny(std::span<Download::EOfferState const> states) const
{
	return std::ranges::any_of(m_downloads.GetOffers(), [states](Download::SGameOffer const& offer)
	{
		return std::ranges::contains(states, offer.state);
	});
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::string> CDownloadWindow::Collect(std::span<Download::EOfferState const> states) const
{
	std::vector<std::string> keys{};

	for (Download::SGameOffer const& offer : m_downloads.GetOffers())
	{
		if (std::ranges::contains(states, offer.state))
		{
			keys.emplace_back(offer.key);
		}
	}

	return keys;
}
} // namespace Lkt::Ui
