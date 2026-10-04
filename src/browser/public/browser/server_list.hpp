#pragma once

#include "browser/server_entry.hpp"
#include "net/query_event.hpp"
#include "net/server_answered.hpp"
#include "net/server_failed.hpp"
#include "query/server_address.hpp"
#include <tge/non_copyable.hpp>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Browser
{
// One game's servers, updated in place by each refresh; its finish drops those not listed again, never a favourite.
class CServerList final : private Tge::SNoCopyNoMove
{
public:

	CServerList() = default;
	~CServerList() = default;

	void BeginRefresh(uint32_t refreshId);
	void SetFavourite(Query::SGameDefinition const& game, Query::SServerAddress const& address, bool isFavourite);
	bool Apply(Query::SGameDefinition const& game, Net::SQueryEvent&& event);

	std::span<SServerEntry const> GetEntries() const;
	SServerEntry const* Find(Query::SServerAddress const& address) const;
	uint32_t GetNumAnswered() const;
	uint32_t GetNumMastersFailed() const;

private:

	SServerEntry& AddListed(Query::SGameDefinition const& game, Query::SServerAddress const& address);
	void ApplyAnswer(Query::SGameDefinition const& game, Net::SServerAnswered&& answer);
	void ApplyFailure(Net::SServerFailed const& failure);
	void FinishSweep(Query::SGameDefinition const& game);
	void Remove(Query::SServerAddress const& address);
	void RebuildIndex();

	std::vector<SServerEntry> m_entries;
	std::unordered_map<uint64_t, size_t> m_indexByKey;
	uint32_t m_acceptedRefreshId{ 0 };
	uint32_t m_sweepRefreshId{ 0 };
	uint32_t m_numMastersFailed{ 0 };
};
} // namespace Browser
} // namespace Lkt
