#include "download_window.hpp"
#include "format_to.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "download/lookout_games.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr float WidthEm{ 44.0f };
constexpr float HeightEm{ 24.0f };
constexpr std::array<Download::EOfferState, 1> Missing{ Download::EOfferState::NotInstalled };
constexpr std::array<Download::EOfferState, 2> Downloadable{ Download::EOfferState::NotInstalled, Download::EOfferState::UpdateAvailable };
constexpr std::array<Download::EOfferState, 1> Updatable{ Download::EOfferState::UpdateAvailable };
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
} // namespace

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::Initialize(std::filesystem::path const& userDir, std::string_view version, std::function<void()> wake)
{
	m_isReady = !userDir.empty() && m_downloads.Initialize(userDir, Download::GetLookoutGamesSource(version), std::move(wake));

	if (!userDir.empty() && !m_isReady)
	{
		gLog.Warning("Games cannot be downloaded: the download could not be set up");
	}
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::Terminate()
{
	m_downloads.Terminate();
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
void CDownloadWindow::DrawGames()
{
	ImGuiTableFlags const flags{ ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY };
	float const height{ -ImGui::GetFrameHeightWithSpacing() * 2.0f };
	SThemeColors const& colors{ GetThemeColors() };

	if (ImGui::BeginTable("##games", 2, flags, ImVec2{ 0.0f, height }))
	{
		ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		for (Download::SGameOffer const& offer : m_downloads.GetOffers())
		{
			bool isSelected{ m_selected.contains(offer.key) };
			bool const isAvailable{ offer.state != Download::EOfferState::NeedsNewerLookout };
			std::string_view const state{ Describe(offer.state) };

			ImGui::PushID(offer.key.c_str());
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::BeginDisabled(!isAvailable);

			if (ImGui::Checkbox("##select", &isSelected))
			{
				if (isSelected)
				{
					m_selected.insert(offer.key);
				}
				else
				{
					m_selected.erase(offer.key);
				}
			}

			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted(offer.name.data(), offer.name.data() + offer.name.size());
			ImGui::EndDisabled();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::PushStyleColor(ImGuiCol_Text, (offer.state == Download::EOfferState::UpdateAvailable) ? colors.amber
				: ((offer.state == Download::EOfferState::Installed) ? colors.text : colors.textDisabled));
			ImGui::TextUnformatted(state.data(), state.data() + state.size());
			ImGui::PopStyleColor();
			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

//////////////////////////////////////////////////////////////////////////
// The keys are collected only on a click, so drawing the buttons allocates nothing.
void CDownloadWindow::DrawButtons()
{
	bool const canDownload{ m_downloads.GetPhase() == Download::EDownloadPhase::Idle && m_downloads.HasIndex() };
	bool const canRemove{ m_downloads.GetPhase() == Download::EDownloadPhase::Idle };

	ImGui::Spacing();

	if (Button("Download all", canDownload && HasAny(Downloadable, false), "Every game not installed or with an update"))
	{
		StartDownload(Collect(Downloadable, false));
	}

	ImGui::SameLine();

	if (Button("Download missing", canDownload && HasAny(Missing, false), "Every game not installed yet"))
	{
		StartDownload(Collect(Missing, false));
	}

	ImGui::SameLine();

	if (Button("Download selected", canDownload && HasAny(Downloadable, true), "The ticked games not installed or with an update"))
	{
		StartDownload(Collect(Downloadable, true));
	}

	ImGui::SameLine();

	if (Button("Update", canDownload && HasAny(Updatable, false), "Every installed game with an update"))
	{
		StartDownload(Collect(Updatable, false));
	}

	ImGui::SameLine();

	if (Button("Remove selected", canRemove && HasAny(Removable, true), "The ticked games that are installed; your own changes to them stay"))
	{
		m_downloads.Remove(Collect(Removable, true));
		m_selected.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CDownloadWindow::StartDownload(std::vector<std::string> const& keys)
{
	m_downloads.Download(keys);
	m_selected.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CDownloadWindow::HasAny(std::span<Download::EOfferState const> states, bool isSelectedOnly) const
{
	return std::ranges::any_of(m_downloads.GetOffers(), [this, states, isSelectedOnly](Download::SGameOffer const& offer)
	{
		return std::ranges::contains(states, offer.state) && (!isSelectedOnly || m_selected.contains(offer.key));
	});
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::string> CDownloadWindow::Collect(std::span<Download::EOfferState const> states, bool isSelectedOnly) const
{
	std::vector<std::string> keys{};

	for (Download::SGameOffer const& offer : m_downloads.GetOffers())
	{
		if (std::ranges::contains(states, offer.state) && (!isSelectedOnly || m_selected.contains(offer.key)))
		{
			keys.emplace_back(offer.key);
		}
	}

	return keys;
}
} // namespace Lkt::Ui
