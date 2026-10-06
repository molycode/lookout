#include "download_window.hpp"
#include "format_to.hpp"
#include "game_icons.hpp"
#include "icon_textures.hpp"
#include "icons.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "widgets.hpp"
#include "browser/text_compare.hpp"
#include "download/lookout_games.hpp"
#include "query/game_catalog.hpp"
#include "query/game_definition.hpp"
#include "query/protocol_definition.hpp"
#include "query/protocol_origin.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr float WidthEm{ 32.0f };
constexpr float HeightEm{ 30.0f };
constexpr float PromptWidthEm{ 22.0f };
constexpr float ProgressBarHeightEm{ 0.4f };
constexpr char const* RemoveAllPopupId{ "Remove all games###removeAll" };
constexpr std::array<Download::EOfferState, 2> Downloadable{ Download::EOfferState::NotInstalled, Download::EOfferState::UpdateAvailable };
constexpr std::array<Download::EOfferState, 3> InstalledStates{ Download::EOfferState::Installed, Download::EOfferState::UpdateAvailable,
	Download::EOfferState::NeedsNewerLookout };
constexpr std::array<Download::EOfferState, 3> WithProtocol{ Download::EOfferState::NotInstalled, Download::EOfferState::Installed,
	Download::EOfferState::UpdateAvailable };

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
float GetIconSize()
{
	return ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeight();
}

//////////////////////////////////////////////////////////////////////////
float GetCardHeight()
{
	return ImGui::GetStyle().FramePadding.y * 2.0f + GetIconSize();
}

//////////////////////////////////////////////////////////////////////////
bool IsDownloadable(Download::SGameOffer const& offer)
{
	return std::ranges::contains(Downloadable, offer.state);
}

//////////////////////////////////////////////////////////////////////////
bool IsRemovable(Download::SGameOffer const& offer)
{
	return offer.isDownloaded;
}

//////////////////////////////////////////////////////////////////////////
std::string_view DescribeOffer(Download::SGameOffer const& offer, std::array<char, 128>& buffer)
{
	std::string_view const state{ Describe(offer.state) };
	Query::SProtocolDefinition const* const pProtocol{ Query::FindProtocol(offer.protocol) };
	std::optional<uint64_t> const installed{ (pProtocol != nullptr) ? pProtocol->downloadedVersion : std::nullopt };
	std::string_view text{ state };

	if (!offer.protocol.empty() && std::ranges::contains(WithProtocol, offer.state))
	{
		if (pProtocol != nullptr && pProtocol->origin != Query::EProtocolOrigin::Downloaded)
		{
			text = FormatTo(buffer, "{} · your own {} in use", state, offer.protocol);
		}
		else if (offer.state == Download::EOfferState::UpdateAvailable && installed.has_value() && offer.protocolVersion.has_value()
			&& *installed != *offer.protocolVersion)
		{
			text = FormatTo(buffer, "{} · {} {} " LKT_ICON_ARROW_RIGHT " {}", state, offer.protocol, *installed, *offer.protocolVersion);
		}
		else if (offer.protocolVersion.has_value())
		{
			text = FormatTo(buffer, "{} · {} {}", state, offer.protocol, *offer.protocolVersion);
		}
		else
		{
			text = FormatTo(buffer, "{} · {}", state, offer.protocol);
		}
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
		&& m_downloads.Initialize(userDir, Download::GetLookoutGamesSource(version), std::move(wake));

	if (!userDir.empty() && !m_isReady)
	{
		gLog.Warning("Games cannot be downloaded: the download could not be set up");
	}
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::Terminate()
{
	m_downloads.Terminate();
	DestroyIcons();
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
		m_shouldFocusSearch = true;
		m_search.clear();
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
			SearchField("##search", LKT_ICON_SEARCH "  Game names", m_search, m_shouldFocusSearch);
			DrawGames();
			DrawButtons();
		}

		ImGui::End();
	}
	else if (!m_icons.empty())
	{
		// A frame after closing, since the frame that closed the window still drew them.
		DestroyIcons();
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
		size_t const numFetched{ m_downloads.GetNumFetched() };
		size_t const numToFetch{ m_downloads.GetNumToFetch() };
		float const fraction{ (numToFetch != 0) ? static_cast<float>(numFetched) / static_cast<float>(numToFetch) : 0.0f };
		std::string_view const progress{ FormatTo(buffer, "Downloading: {} of {} files…", numFetched, numToFetch) };

		ImGui::TextUnformatted(progress.data(), progress.data() + progress.size());
		// A label over the bar would end up light text on amber, which cannot be read.
		ImGui::ProgressBar(fraction, ImVec2{ -FLT_MIN, ProgressBarHeightEm * ImGui::GetFontSize() }, "");
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
		std::span<Download::SGameOffer const> const offers{ m_downloads.GetOffers() };
		std::vector<Download::SGameOffer const*> shown{};
		ImGuiListClipper clipper{};

		for (Download::SGameOffer const& offer : offers)
		{
			if (Browser::ContainsIgnoringCase(offer.name, m_search) || Browser::ContainsIgnoringCase(offer.key, m_search))
			{
				shown.emplace_back(&offer);
			}
		}

		// Given rather than measured: measuring draws the first card wherever the list is scrolled to.
		clipper.Begin(static_cast<int>(shown.size()), GetCardHeight() + ImGui::GetStyle().ItemSpacing.y);

		while (clipper.Step())
		{
			for (int index{ clipper.DisplayStart }; index < clipper.DisplayEnd; ++index)
			{
				DrawGame(*shown[static_cast<size_t>(index)], toDownload, toRemove);
			}
		}

		if (shown.empty() && !offers.empty())
		{
			ImGui::TextDisabled("No game matches \"%s\".", m_search.c_str());
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
	bool const canRemove{ IsRemovable(offer) };
	float const numButtons{ static_cast<float>(static_cast<int>(canGet) + static_cast<int>(canUpdate) + static_cast<int>(canRemove)) };
	float const lineHeight{ ImGui::GetTextLineHeight() };
	ImVec2 const padding{ style.FramePadding };
	ImVec2 const start{ ImGui::GetCursorScreenPos() };
	float const iconSize{ GetIconSize() };
	ImVec2 const size{ ImGui::GetContentRegionAvail().x, GetCardHeight() };
	ImVec2 const icon{ start.x + padding.x, start.y + padding.y };
	ImVec2 const text{ icon.x + iconSize + style.ItemInnerSpacing.x, icon.y };
	float const buttonsX{ start.x + size.x - padding.x - numButtons * lineHeight - std::max(numButtons - 1.0f, 0.0f) * style.ItemInnerSpacing.x };
	std::array<char, 128> stateBuffer{};
	std::string_view const stateText{ isDownloading ? std::string_view{ "Downloading…" } : DescribeOffer(offer, stateBuffer) };
	ImDrawList* const pDrawList{ ImGui::GetWindowDrawList() };
	Query::SGameDefinition const* const pInstalled{ offer.isDownloaded ? Query::FindGame(offer.key) : nullptr };
	std::array<char, 128> buffer{};

	ImGui::PushID(offer.key.c_str());
	ImGui::BeginGroup();
	ImGui::Dummy(size);
	pDrawList->AddRectFilled(start, ImVec2{ start.x + size.x, start.y + size.y }, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetFontSize() * CardRoundingEm);

	if (pInstalled != nullptr)
	{
		gGameIcons.Draw(pDrawList, pInstalled->game, icon, iconSize);
	}
	else
	{
		DrawIcon(pDrawList, FindIcon(offer), icon, iconSize);
	}

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

	if (Button("Download all", CanDownload() && HasAny(&IsDownloadable), "Every game not installed or with an update"))
	{
		m_downloads.Download(Collect(&IsDownloadable));
	}

	ImGui::SameLine();

	if (Button("Remove all", isIdle && HasAny(&IsRemovable), "Every downloaded game; your own changes to them stay"))
	{
		ImGui::OpenPopup(RemoveAllPopupId);
	}

	DrawInstalledCount();
	DrawRemoveAllPrompt();
}

//////////////////////////////////////////////////////////////////////////
// Out of what lookout-games offers, so a game it no longer offers is not counted; before the list is read, only the
// installed are known.
void CDownloadWindow::DrawInstalledCount() const
{
	std::span<Download::SGameOffer const> const offers{ m_downloads.GetOffers() };
	size_t const numInstalled{ static_cast<size_t>(std::ranges::count_if(offers, [](Download::SGameOffer const& offer)
	{
		return offer.isDownloaded && std::ranges::contains(InstalledStates, offer.state);
	})) };
	size_t const numOffered{ static_cast<size_t>(std::ranges::count_if(offers, [](Download::SGameOffer const& offer)
	{
		return offer.state != Download::EOfferState::Withdrawn;
	})) };
	std::array<char, 64> buffer{};
	std::string_view const text{ m_downloads.HasIndex() ? FormatTo(buffer, "{} of {} games installed", numInstalled, numOffered)
		: FormatTo(buffer, "{} {} installed", numInstalled, (numInstalled == 1) ? "game" : "games") };
	float const width{ ImGui::CalcTextSize(text.data(), text.data() + text.size()).x };

	ImGui::SameLine();
	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width));
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%.*s", static_cast<int>(text.size()), text.data());
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
			m_downloads.Remove(Collect(&IsRemovable));
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
		m_downloads.RequestIcon(offer.key);

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
void CDownloadWindow::DestroyIcons()
{
	for (auto& [hash, levels] : m_icons)
	{
		DestroyIconLevels(levels);
	}

	m_icons.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CDownloadWindow::CanDownload() const
{
	return m_downloads.GetPhase() == Download::EDownloadPhase::Idle && m_downloads.HasIndex();
}

//////////////////////////////////////////////////////////////////////////
bool CDownloadWindow::HasAny(bool (*pIsChosen)(Download::SGameOffer const&)) const
{
	return std::ranges::any_of(m_downloads.GetOffers(), pIsChosen);
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::string> CDownloadWindow::Collect(bool (*pIsChosen)(Download::SGameOffer const&)) const
{
	std::vector<std::string> keys{};

	for (Download::SGameOffer const& offer : m_downloads.GetOffers())
	{
		if (pIsChosen(offer))
		{
			keys.emplace_back(offer.key);
		}
	}

	return keys;
}
} // namespace Lkt::Ui
