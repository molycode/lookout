#include "browser/server_list.hpp"
#include "geo/countries.hpp"
#include "query/game_definition.hpp"
#include "query/styled_text.hpp"
#include <algorithm>
#include <type_traits>
#include <variant>

namespace Lkt::Browser
{
namespace
{
constexpr char SearchFieldSeparator{ '\n' };

//////////////////////////////////////////////////////////////////////////
std::string BuildSearchText(SServerEntry const& entry)
{
	std::string text{ Query::FormatAddress(entry.address) };

	for (std::string_view const field : { std::string_view{ entry.summary.name.plain }, std::string_view{ entry.summary.map },
		std::string_view{ entry.summary.mod }, std::string_view{ entry.summary.mode } })
	{
		text += SearchFieldSeparator;
		text += field;
	}

	if (entry.country != Geo::NoCountry)
	{
		Geo::SCountry const country{ Geo::GetCountry(entry.country) };

		text += SearchFieldSeparator;
		text += country.code;
		text += SearchFieldSeparator;
		text += country.name;
	}

	for (Query::SStyledText const& name : entry.playerNames)
	{
		text += SearchFieldSeparator;
		text += name.plain;
	}

	return text;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CServerList::BeginRefresh(uint32_t refreshId)
{
	m_acceptedRefreshId = refreshId;
	m_sweepRefreshId = refreshId;
	m_numMastersFailed = 0;

	for (SServerEntry& entry : m_entries)
	{
		entry.isStale = !entry.isFavourite;
	}
}

//////////////////////////////////////////////////////////////////////////
void CServerList::SetFavourite(Query::SServerAddress const& address, bool isFavourite)
{
	if (isFavourite)
	{
		AddListed(address).isFavourite = true;
	}
	else
	{
		auto const it{ m_indexByKey.find(Query::ToKey(address)) };

		if (it != m_indexByKey.end())
		{
			m_entries[it->second].isFavourite = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CServerList::Apply(Query::SGameDefinition const& game, Net::SQueryEvent&& event)
{
	bool isFinished{ false };

	std::visit([this, &game, &isFinished](auto& typed)
	{
		using TEvent = std::decay_t<decltype(typed)>;

		if (typed.game == game.game && typed.refreshId >= m_acceptedRefreshId)
		{
			if constexpr (std::is_same_v<TEvent, Net::SServersListed>)
			{
				for (Query::SServerAddress const& address : typed.servers)
				{
					AddListed(address);
				}
			}
			else if constexpr (std::is_same_v<TEvent, Net::SMasterFailed>)
			{
				if (typed.refreshId == m_sweepRefreshId)
				{
					++m_numMastersFailed;
				}
			}
			else if constexpr (std::is_same_v<TEvent, Net::SServerAnswered>)
			{
				ApplyAnswer(game, std::move(typed));
			}
			else if constexpr (std::is_same_v<TEvent, Net::SServerFailed>)
			{
				ApplyFailure(typed);
			}
			else if constexpr (std::is_same_v<TEvent, Net::SRefreshFinished>)
			{
				if (typed.refreshId == m_sweepRefreshId)
				{
					FinishSweep(game);
				}

				isFinished = true;
			}
		}
	}, event);

	return isFinished;
}

//////////////////////////////////////////////////////////////////////////
std::span<SServerEntry const> CServerList::GetEntries() const
{
	return m_entries;
}

//////////////////////////////////////////////////////////////////////////
SServerEntry const* CServerList::Find(Query::SServerAddress const& address) const
{
	auto const it{ m_indexByKey.find(Query::ToKey(address)) };

	return (it != m_indexByKey.end()) ? &m_entries[it->second] : nullptr;
}

//////////////////////////////////////////////////////////////////////////
uint32_t CServerList::GetNumAnswered() const
{
	return static_cast<uint32_t>(std::ranges::count(m_entries, EServerState::Online, &SServerEntry::state));
}

//////////////////////////////////////////////////////////////////////////
uint32_t CServerList::GetNumMastersFailed() const
{
	return m_numMastersFailed;
}

//////////////////////////////////////////////////////////////////////////
SServerEntry& CServerList::AddListed(Query::SServerAddress const& address)
{
	auto const [it, isNew]{ m_indexByKey.try_emplace(Query::ToKey(address), m_entries.size()) };

	if (isNew)
	{
		SServerEntry entry{};

		entry.address = address;
		entry.country = Geo::FindCountry(address.ipv4);
		m_entries.emplace_back(std::move(entry));
	}

	SServerEntry& entry{ m_entries[it->second] };

	entry.isStale = false;

	return entry;
}

//////////////////////////////////////////////////////////////////////////
// Another game on the same masters is dropped, since it cannot be joined; a favourite stays, as a bad reply.
void CServerList::ApplyAnswer(Query::SGameDefinition const& game, Net::SServerAnswered&& answer)
{
	Query::SServerSummary summary{ Query::Summarize(game, answer.reply) };
	auto const it{ m_indexByKey.find(Query::ToKey(answer.address)) };
	bool const isFavourite{ it != m_indexByKey.end() && m_entries[it->second].isFavourite };

	if (summary.isForeign && isFavourite)
	{
		SServerEntry& entry{ m_entries[it->second] };

		entry.state = EServerState::BadReply;
		entry.isStale = false;
	}
	else if (summary.isForeign)
	{
		Remove(answer.address);
	}
	else
	{
		SServerEntry& entry{ AddListed(answer.address) };

		entry.state = EServerState::Online;
		entry.pingMs = answer.pingMs;
		entry.summary = std::move(summary);
		entry.reply = std::move(answer.reply);
		entry.playerNames.clear();

		for (Query::SPlayer const& player : entry.reply.players)
		{
			entry.playerNames.emplace_back(Query::DecodeText(game.text, player.name));
		}

		entry.searchText = BuildSearchText(entry);
	}
}

//////////////////////////////////////////////////////////////////////////
void CServerList::ApplyFailure(Net::SServerFailed const& failure)
{
	auto const it{ m_indexByKey.find(Query::ToKey(failure.address)) };

	if (it != m_indexByKey.end())
	{
		m_entries[it->second].state = (failure.failure == Net::EServerFailure::NoAnswer) ? EServerState::NoAnswer : EServerState::BadReply;
	}
}

//////////////////////////////////////////////////////////////////////////
// When every master failed, nothing was listed again, so dropping the unlisted would empty the table offline.
void CServerList::FinishSweep(Query::SGameDefinition const& game)
{
	if (m_numMastersFailed < game.masters.size())
	{
		std::erase_if(m_entries, [](SServerEntry const& entry) { return entry.isStale; });
		RebuildIndex();
	}
	else
	{
		for (SServerEntry& entry : m_entries)
		{
			entry.isStale = false;
		}
	}

	m_sweepRefreshId = 0;
}

//////////////////////////////////////////////////////////////////////////
void CServerList::Remove(Query::SServerAddress const& address)
{
	auto const it{ m_indexByKey.find(Query::ToKey(address)) };

	if (it != m_indexByKey.end())
	{
		size_t const index{ it->second };

		m_indexByKey.erase(it);

		if (index + 1 != m_entries.size())
		{
			m_entries[index] = std::move(m_entries.back());
			m_indexByKey[Query::ToKey(m_entries[index].address)] = index;
		}

		m_entries.pop_back();
	}
}

//////////////////////////////////////////////////////////////////////////
void CServerList::RebuildIndex()
{
	m_indexByKey.clear();

	for (size_t index{ 0 }; index < m_entries.size(); ++index)
	{
		m_indexByKey.emplace(Query::ToKey(m_entries[index].address), index);
	}
}
} // namespace Lkt::Browser
