#include "server_rows.hpp"
#include "browser/text_compare.hpp"
#include <algorithm>
#include <compare>

namespace Lkt::Browser
{
namespace
{
//////////////////////////////////////////////////////////////////////////
bool PassesFilter(SServerEntry const& entry, Config::SServerFilter const& filter)
{
	Query::SServerSummary const& summary{ entry.summary };
	bool const isEmpty{ summary.numPlayers == 0 };
	bool const isFull{ summary.maxPlayers > 0 && summary.numPlayers >= summary.maxPlayers };

	return ContainsIgnoringCase(entry.searchText, filter.search)
		&& (filter.showEmpty || !isEmpty)
		&& (filter.showFull || !isFull)
		&& (filter.maxPingMs == Config::NoPingLimit || entry.pingMs <= filter.maxPingMs)
		&& (filter.mod.empty() || EqualsIgnoringCase(summary.mod, filter.mod));
}

//////////////////////////////////////////////////////////////////////////
std::weak_ordering CompareColumn(SServerEntry const& lhs, SServerEntry const& rhs, Config::ESortColumn column)
{
	std::weak_ordering order{ std::weak_ordering::equivalent };

	switch (column)
	{
		case Config::ESortColumn::Name:
			order = CompareIgnoringCase(lhs.summary.name.plain, rhs.summary.name.plain);
			break;
		case Config::ESortColumn::Map:
			order = CompareIgnoringCase(lhs.summary.map, rhs.summary.map);
			break;
		case Config::ESortColumn::Mod:
			order = CompareIgnoringCase(lhs.summary.mod, rhs.summary.mod);
			break;
		case Config::ESortColumn::Mode:
			order = CompareIgnoringCase(lhs.summary.mode, rhs.summary.mode);
			break;
		case Config::ESortColumn::Players:
			order = lhs.summary.numPlayers <=> rhs.summary.numPlayers;
			break;
		case Config::ESortColumn::Ping:
			order = lhs.pingMs <=> rhs.pingMs;
			break;
		case Config::ESortColumn::Favourite:
			order = lhs.isFavourite <=> rhs.isFavourite;
			break;
		case Config::ESortColumn::Password:
			order = lhs.summary.hasPassword <=> rhs.summary.hasPassword;
			break;
	}

	return order;
}

//////////////////////////////////////////////////////////////////////////
std::weak_ordering Then(std::weak_ordering first, std::weak_ordering next)
{
	return (first != 0) ? first : next;
}

//////////////////////////////////////////////////////////////////////////
// Only the column flips with the direction: flipping a whole "less" would make equal rows each less than the other.
bool IsBefore(SServerEntry const& lhs, SServerEntry const& rhs, Config::SSortOrder const& sort)
{
	bool const isFavouriteSort{ sort.column == Config::ESortColumn::Favourite };
	bool const isGroupSort{ isFavouriteSort || sort.column == Config::ESortColumn::Password };
	std::weak_ordering byColumn{ CompareColumn(lhs, rhs, sort.column) };

	if (!sort.isAscending)
	{
		byColumn = 0 <=> byColumn;
	}

	std::weak_ordering const onlineFirst{ (rhs.state == EServerState::Online) <=> (lhs.state == EServerState::Online) };
	// Favourites group ahead of the online rule, or an unanswered favourite would fall below every server that answered.
	std::weak_ordering const grouped{ isFavouriteSort ? Then(byColumn, onlineFirst) : Then(onlineFirst, byColumn) };
	std::weak_ordering const morePlayersFirst{ isGroupSort ? (rhs.summary.numPlayers <=> lhs.summary.numPlayers) : std::weak_ordering::equivalent };

	return Then(Then(grouped, morePlayersFirst), Query::ToKey(lhs.address) <=> Query::ToKey(rhs.address)) < 0;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void BuildRows(std::span<SServerEntry const> entries, Config::SServerFilter const& filter, Config::SSortOrder const& sort, std::vector<uint32_t>& rows)
{
	rows.clear();

	for (size_t index{ 0 }; index < entries.size(); ++index)
	{
		SServerEntry const& entry{ entries[index] };

		if (entry.isFavourite || (entry.state == EServerState::Online && PassesFilter(entry, filter)))
		{
			rows.emplace_back(static_cast<uint32_t>(index));
		}
	}

	std::ranges::sort(rows, [&sort](SServerEntry const& lhs, SServerEntry const& rhs)
	{
		return IsBefore(lhs, rhs, sort);
	}, [entries](uint32_t index) -> SServerEntry const&
	{
		return entries[index];
	});
}
} // namespace Lkt::Browser
