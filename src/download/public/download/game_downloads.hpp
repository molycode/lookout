#pragma once

#include "download/download_phase.hpp"
#include "download/download_source.hpp"
#include "download/game_offer.hpp"
#include "download/index_icons.hpp"
#include "net/fetch_result.hpp"
#include "net/https_fetcher.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Download
{
struct SGameIndex;

// Lists, downloads, updates and removes the games of a lookout-games repository in the downloaded folder of userDir,
// which nothing else writes. Used from one thread: the callback only says that Update has something to take.
class CGameDownloads final : private Tge::SNoCopyNoMove
{
public:

	CGameDownloads();
	~CGameDownloads();

	bool Initialize(std::filesystem::path const& userDir, SDownloadSource source, EIndexIcons icons, std::function<void()> onResults);
	void Terminate();

	void ReadIndex();
	// Each game with its protocol, at the commit the index names; the index must have been read.
	void Download(std::span<std::string const> keys);
	void Remove(std::span<std::string const> keys);

	// True when the downloaded games changed, so they are to be loaded again.
	bool Update();

	EDownloadPhase GetPhase() const;
	bool HasIndex() const;
	std::span<SGameOffer const> GetOffers() const;
	std::string_view GetIcon(std::string_view sha256) const;
	// What went wrong in the last reading, download or removal, each naming its game or file.
	std::span<std::string const> GetProblems() const;
	size_t GetNumFetched() const;
	size_t GetNumToFetch() const;

private:

	void TakeIndex(Net::SFetchResult result);
	void RequestIcons();
	void TakeIcon(Net::SFetchResult result, std::string_view sha256);
	void InstallFetched();
	void RefreshOffers();
	std::string MakePath(std::string_view reference, std::string_view file) const;
	void AddProblem(std::string problem);

	std::filesystem::path m_userDir;
	std::filesystem::path m_downloadedDir;
	SDownloadSource m_source;
	Net::CHttpsFetcher m_fetcher;
	std::unique_ptr<SGameIndex> m_pIndex;
	std::vector<SGameOffer> m_offers;
	std::vector<std::string> m_problems;
	std::vector<std::string> m_downloadKeys;
	std::map<std::string, Net::SFetchResult> m_fetched;
	std::map<std::string, std::string, std::less<>> m_pendingIconHashes;
	std::map<std::string, std::string, std::less<>> m_iconsByHash;
	size_t m_numToFetch{ 0 };
	EDownloadPhase m_phase{ EDownloadPhase::Idle };
	EIndexIcons m_indexIcons{ EIndexIcons::Skip };
	bool m_hasChanged{ false };
};
} // namespace Lkt::Download
