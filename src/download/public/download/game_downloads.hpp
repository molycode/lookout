#pragma once

#include "download/download_phase.hpp"
#include "download/download_source.hpp"
#include "download/game_offer.hpp"
#include "net/fetch_result.hpp"
#include "net/https_fetcher.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Download
{
struct SGameIndex;
struct SIndexFile;

// Lists, downloads, updates and removes the games of a lookout-games repository in the downloaded folder of userDir,
// which nothing else writes. Used from one thread: the callback only says that Update has something to take.
class CGameDownloads final : private Tge::SNoCopyNoMove
{
public:

	CGameDownloads();
	~CGameDownloads();

	bool Initialize(std::filesystem::path const& userDir, std::filesystem::path const& cacheDir, SDownloadSource source, std::function<void()> onResults);
	void Terminate();

	void ReadIndex();
	// Each game with its protocol, at the commit the index names; the index must have been read.
	void Download(std::span<std::string const> keys);
	void Remove(std::span<std::string const> keys);
	void RequestIcon(std::string_view key);

	// True when the downloaded games changed, so they are to be loaded again.
	bool Update();

	EDownloadPhase GetPhase() const;
	bool HasIndex() const;
	std::span<SGameOffer const> GetOffers() const;
	std::string_view GetIcon(std::string_view sha256) const;
	std::span<std::string const> GetDownloadKeys() const;
	// What went wrong in the last reading, download or removal, each naming its game or file.
	std::span<std::string const> GetProblems() const;
	std::string_view GetLookoutVersion() const;
	size_t GetNumFetched() const;
	size_t GetNumToFetch() const;

private:

	void TakeIndex(Net::SFetchResult result);
	void PruneIcons() const;
	void TakeIcon(Net::SFetchResult result, std::string_view sha256);
	std::optional<std::string> FindHeldIcon(SIndexFile const& icon) const;
	void InstallFetched();
	void RefreshOffers();
	std::string MakePath(std::string_view reference, std::string_view file) const;
	void AddProblem(std::string problem);

	std::filesystem::path m_userDir;
	std::filesystem::path m_downloadedDir;
	std::filesystem::path m_iconDir;
	SDownloadSource m_source;
	Net::CHttpsFetcher m_fetcher;
	std::unique_ptr<SGameIndex> m_pIndex;
	std::vector<SGameOffer> m_offers;
	std::vector<std::string> m_problems;
	std::vector<std::string> m_downloadKeys;
	std::map<std::string, Net::SFetchResult> m_fetched;
	std::map<std::string, std::string, std::less<>> m_pendingIconHashes;
	std::map<std::string, std::string, std::less<>> m_iconsByHash;
	std::set<std::string, std::less<>> m_failedIconHashes;
	size_t m_numToFetch{ 0 };
	EDownloadPhase m_phase{ EDownloadPhase::Idle };
	bool m_hasChanged{ false };
};
} // namespace Lkt::Download
