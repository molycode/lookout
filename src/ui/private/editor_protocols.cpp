#include "editor_protocols.hpp"
#include "loggers.hpp"
#include "theme.hpp"
#include "theme_colors.hpp"
#include "download/game_downloads.hpp"
#include "games/game_files.hpp"
#include "games/game_form.hpp"
#include "games/game_form_node.hpp"
#include "games/load_games.hpp"
#include "query/game_catalog.hpp"
#include <imgui.h>
#include <algorithm>
#include <expected>
#include <format>
#include <utility>

namespace Lkt::Ui
{
namespace
{
constexpr std::string_view MastersField{ "masters" };
constexpr std::string_view ProtocolField{ "protocol" };

//////////////////////////////////////////////////////////////////////////
bool IsOffered(Download::CGameDownloads const& downloads, std::string_view name)
{
	return std::ranges::any_of(downloads.GetProtocolOffers(), [name](Download::SProtocolOffer const& offer)
	{
		return offer.name == name && offer.state != Download::EOfferState::NeedsNewerLookout;
	});
}

//////////////////////////////////////////////////////////////////////////
bool IsIdle(Download::CGameDownloads const& downloads)
{
	return downloads.GetPhase() == Download::EDownloadPhase::Idle;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
// A protocol fetched gives way to the catalog's own copy once a reload brings it.
void CEditorProtocols::Refresh()
{
	std::span<Query::SProtocolDefinition const> const catalog{ Query::GetProtocolCatalog() };

	std::erase_if(m_fetched, [catalog](Query::SProtocolDefinition const& fetched)
	{
		return std::ranges::contains(catalog, fetched.name, &Query::SProtocolDefinition::name);
	});
	m_definitions.assign(catalog.begin(), catalog.end());
	m_definitions.insert(m_definitions.end(), m_fetched.begin(), m_fetched.end());
	std::ranges::sort(m_definitions, {}, &Query::SProtocolDefinition::name);
}

//////////////////////////////////////////////////////////////////////////
// One protocol at a time, once nothing else downloads; true when one arrived.
bool CEditorProtocols::Update(Download::CGameDownloads* pDownloads, std::filesystem::path const& userDir, Games::SGameFormNode const& form)
{
	bool hasArrived{ false };

	if (pDownloads != nullptr && IsIdle(*pDownloads))
	{
		hasArrived = !m_pending.empty() && TakeFetched(*pDownloads, userDir);
		m_pending = FindWanted(*pDownloads, form);

		if (!m_pending.empty())
		{
			pDownloads->DownloadProtocol(m_pending);
			// One already on disk and current is not fetched at all.
			hasArrived = (IsIdle(*pDownloads) && TakeFetched(*pDownloads, userDir)) || hasArrived;
		}
	}

	return hasArrived;
}

//////////////////////////////////////////////////////////////////////////
std::span<Query::SProtocolDefinition const> CEditorProtocols::GetDefinitions() const
{
	return m_definitions;
}

//////////////////////////////////////////////////////////////////////////
SFormContext CEditorProtocols::MakeFormContext(Download::CGameDownloads* pDownloads)
{
	return SFormContext{ m_definitions,
		[this, pDownloads](std::string& protocol) { return DrawMoreChoices(protocol, pDownloads); },
		[this, pDownloads](std::string_view protocol) { DrawAbsent(protocol, pDownloads); }, {} };
}

//////////////////////////////////////////////////////////////////////////
// lookout-games' index is read when a list first wants it, so an editor that never offers these asks for nothing.
bool CEditorProtocols::DrawMoreChoices(std::string& protocol, Download::CGameDownloads* pDownloads)
{
	std::vector<Download::SProtocolOffer const*> offers{};
	bool isChosen{ false };

	if (pDownloads != nullptr && !pDownloads->HasIndex() && IsIdle(*pDownloads) && !m_isIndexAsked)
	{
		pDownloads->ReadIndex();
		m_isIndexAsked = true;
	}

	if (pDownloads != nullptr && pDownloads->HasIndex())
	{
		for (Download::SProtocolOffer const& offer : pDownloads->GetProtocolOffers())
		{
			if (!IsDefined(offer.name))
			{
				offers.emplace_back(&offer);
			}
		}
	}

	bool const isReading{ pDownloads != nullptr && !pDownloads->HasIndex() && !IsIdle(*pDownloads) };
	bool const isUnreachable{ pDownloads != nullptr && !pDownloads->HasIndex() && IsIdle(*pDownloads) };

	if (!offers.empty() || isReading || isUnreachable)
	{
		ImGui::SeparatorText("From lookout-games, downloaded when chosen");
	}

	for (Download::SProtocolOffer const* pOffer : offers)
	{
		if (pOffer->state == Download::EOfferState::NeedsNewerLookout)
		{
			ImGui::BeginDisabled();
			ImGui::Selectable(std::format("{} · needs a newer Lookout", pOffer->name).c_str(), false);
			ImGui::EndDisabled();
		}
		else if (ImGui::Selectable(pOffer->name.c_str(), pOffer->name == protocol))
		{
			protocol = pOffer->name;
			m_failures.erase(pOffer->name);
			isChosen = true;
		}
	}

	if (isReading)
	{
		ImGui::TextDisabled("Looking at lookout-games…");
	}
	else if (isUnreachable)
	{
		ImGui::TextDisabled("lookout-games could not be reached");

		if (ImGui::SmallButton("Try again"))
		{
			pDownloads->ReadIndex();
		}
	}

	return isChosen;
}

//////////////////////////////////////////////////////////////////////////
void CEditorProtocols::DrawAbsent(std::string_view protocol, Download::CGameDownloads const* pDownloads)
{
	auto const failure{ m_failures.find(protocol) };

	ImGui::PushTextWrapPos(0.0f);

	if (failure != m_failures.end())
	{
		std::string const text{ std::format("Cannot get {} from lookout-games: {}", protocol, failure->second) };

		ImGui::PushStyleColor(ImGuiCol_Text, GetThemeColors().error);
		ImGui::TextUnformatted(text.c_str());
		ImGui::PopStyleColor();

		if (ImGui::SmallButton("Try again"))
		{
			m_failures.erase(failure);
		}
	}
	else if (protocol == m_pending)
	{
		ImGui::TextDisabled("%s", std::format("Downloading {} from lookout-games…", protocol).c_str());
	}
	else if (pDownloads != nullptr && !IsIdle(*pDownloads) && IsOffered(*pDownloads, protocol))
	{
		ImGui::TextDisabled("Waiting for another download to finish…");
	}

	ImGui::PopTextWrapPos();
}

//////////////////////////////////////////////////////////////////////////
// The first protocol a master names that the editor lacks and lookout-games offers, unless getting it failed.
std::string CEditorProtocols::FindWanted(Download::CGameDownloads const& downloads, Games::SGameFormNode const& form) const
{
	Games::SGameFormNode const* const pMasters{ Games::FindFormField(form, MastersField) };
	std::string wanted{};

	if (pMasters != nullptr)
	{
		for (Games::SGameFormNode const& master : pMasters->children)
		{
			Games::SGameFormNode const* const pProtocol{ Games::FindFormField(master, ProtocolField) };
			std::string_view const name{ (pProtocol != nullptr) ? std::string_view{ pProtocol->text } : std::string_view{} };

			if (wanted.empty() && !name.empty() && !IsDefined(name) && !m_failures.contains(name) && IsOffered(downloads, name))
			{
				wanted = name;
			}
		}
	}

	return wanted;
}

//////////////////////////////////////////////////////////////////////////
// The downloads already warned of a protocol that could not be downloaded.
bool CEditorProtocols::TakeFetched(Download::CGameDownloads const& downloads, std::filesystem::path const& userDir)
{
	std::span<Download::SProtocolOffer const> const offers{ downloads.GetProtocolOffers() };
	auto const offer{ std::ranges::find(offers, m_pending, &Download::SProtocolOffer::name) };
	std::expected<Query::SProtocolDefinition, std::string> fetched{ std::unexpected{ (offer != offers.end() && !offer->problem.empty())
		? offer->problem : std::string{ "lookout-games no longer offers it" } } };

	if (offer != offers.end() && offer->state == Download::EOfferState::Installed)
	{
		fetched = Games::LoadDownloadedProtocol(Games::GetDownloadedDir(userDir), m_pending);

		if (!fetched.has_value())
		{
			gLog.Warning("Cannot use the protocol {} just downloaded: {}", m_pending, fetched.error());
		}
	}

	bool const hasArrived{ fetched.has_value() };

	if (hasArrived)
	{
		m_fetched.emplace_back(std::move(*fetched));
		Refresh();
	}
	else
	{
		m_failures.insert_or_assign(m_pending, std::move(fetched.error()));
	}

	m_pending.clear();

	return hasArrived;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorProtocols::IsDefined(std::string_view name) const
{
	return std::ranges::contains(m_definitions, name, &Query::SProtocolDefinition::name);
}
} // namespace Lkt::Ui
