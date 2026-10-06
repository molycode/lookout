#include "download/game_downloads.hpp"
#include "downloaded_files.hpp"
#include "game_index.hpp"
#include "loggers.hpp"
#include "read_index.hpp"
#include "sha256.hpp"
#include "games/game_files.hpp"
#include "games/game_format.hpp"
#include "script/script_api.hpp"
#include <tge/assert.hpp>
#include <algorithm>
#include <expected>
#include <format>
#include <ranges>
#include <set>
#include <string_view>
#include <utility>

namespace Lkt::Download
{
namespace
{
constexpr size_t MaxIndexSize{ 1024 * 1024 };
constexpr std::string_view IndexBranch{ "main" };
constexpr std::string_view IndexFile{ "index.json" };
constexpr std::string_view IconFile{ "icon.png" };

//////////////////////////////////////////////////////////////////////////
bool IsSupported(SIndexGame const& game, SIndexProtocol const& protocol)
{
	return game.format <= Games::GameFormat && protocol.api <= Script::ScriptApi;
}

//////////////////////////////////////////////////////////////////////////
SIndexGame const* FindGame(SGameIndex const& index, std::string_view key)
{
	auto const it{ std::ranges::find(index.games, key, &SIndexGame::key) };

	return (it != index.games.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
SIndexProtocol const* FindProtocol(SGameIndex const& index, std::string_view name)
{
	auto const it{ std::ranges::find(index.protocols, name, &SIndexProtocol::name) };

	return (it != index.protocols.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
SIndexFile const* FindIcon(SIndexGame const& game)
{
	auto const it{ std::ranges::find(game.files, IconFile, &SIndexFile::name) };

	return (it != game.files.end()) ? &*it : nullptr;
}

//////////////////////////////////////////////////////////////////////////
// The body, once it is what the index says, byte for byte.
std::expected<std::string const*, std::string> Verify(Net::SFetchResult const& result, SIndexFile const& file)
{
	std::expected<std::string const*, std::string> verified{ std::unexpected{ std::format("{} differs from what the index says it is", file.name) } };

	if (!result.body.has_value())
	{
		verified = std::unexpected{ result.body.error() };
	}
	else if (result.body->size() == file.size && HashSha256(*result.body) == file.sha256)
	{
		verified = &*result.body;
	}

	return verified;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
CGameDownloads::CGameDownloads() = default;

//////////////////////////////////////////////////////////////////////////
CGameDownloads::~CGameDownloads() = default;

//////////////////////////////////////////////////////////////////////////
bool CGameDownloads::Initialize(std::filesystem::path const& userDir, SDownloadSource source, EIndexIcons icons, std::function<void()> onResults)
{
	m_userDir = userDir;
	m_downloadedDir = Games::GetDownloadedDir(userDir);
	m_source = std::move(source);
	m_indexIcons = icons;

	bool const isReady{ !m_downloadedDir.empty() && m_fetcher.Initialize(m_source.origin, std::move(onResults)) };

	RefreshOffers();

	return isReady;
}

//////////////////////////////////////////////////////////////////////////
void CGameDownloads::Terminate()
{
	m_fetcher.Terminate();
	m_pIndex.reset();
	m_offers.clear();
	m_fetched.clear();
	m_pendingIconHashes.clear();
	m_iconsByHash.clear();
	m_phase = EDownloadPhase::Idle;
}

//////////////////////////////////////////////////////////////////////////
void CGameDownloads::ReadIndex()
{
	if (m_phase == EDownloadPhase::Idle)
	{
		m_problems.clear();
		m_phase = EDownloadPhase::ReadingIndex;
		m_fetcher.Fetch({ Net::SFetchRequest{ MakePath(IndexBranch, IndexFile), MaxIndexSize } });
	}
}

//////////////////////////////////////////////////////////////////////////
// Each file is capped at the size the index gives it, so a larger reply is refused before it is read.
void CGameDownloads::Download(std::span<std::string const> keys)
{
	TGE_ASSERT(m_pIndex != nullptr && m_phase == EDownloadPhase::Idle, "Games are downloaded once the index is read and nothing else runs");

	std::vector<Net::SFetchRequest> requests{};
	std::set<std::string> protocols{};

	m_problems.clear();
	m_downloadKeys.clear();
	m_fetched.clear();

	for (std::string const& key : keys)
	{
		SIndexGame const* const pGame{ FindGame(*m_pIndex, key) };
		SIndexProtocol const* const pProtocol{ (pGame != nullptr) ? FindProtocol(*m_pIndex, pGame->protocol) : nullptr };

		if (pProtocol != nullptr && IsSupported(*pGame, *pProtocol))
		{
			m_downloadKeys.emplace_back(key);

			for (SIndexFile const& file : pGame->files)
			{
				requests.emplace_back(MakePath(m_pIndex->commit, std::format("games/{}/{}", key, file.name)), file.size);
			}

			if (!IsProtocolCurrent(m_downloadedDir, *pProtocol) && protocols.insert(pProtocol->name).second)
			{
				requests.emplace_back(MakePath(m_pIndex->commit, std::format("protocols/{}", pProtocol->file.name)), pProtocol->file.size);
			}
		}
	}

	m_numToFetch = requests.size();

	if (!requests.empty())
	{
		m_phase = EDownloadPhase::Downloading;
		m_fetcher.Fetch(std::move(requests));
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameDownloads::Remove(std::span<std::string const> keys)
{
	TGE_ASSERT(m_phase == EDownloadPhase::Idle, "Games are removed while nothing else runs");

	m_problems.clear();

	for (std::string const& key : keys)
	{
		std::expected<void, std::string> const removed{ RemoveGame(m_downloadedDir, key) };

		if (!removed.has_value())
		{
			AddProblem(removed.error());
		}
	}

	for (std::string& problem : RemoveUnusedProtocols(m_downloadedDir, m_userDir))
	{
		AddProblem(std::move(problem));
	}

	gLog.Info("Removed {} downloaded {}", keys.size(), (keys.size() == 1) ? "game" : "games");
	m_hasChanged = true;
	RefreshOffers();
}

//////////////////////////////////////////////////////////////////////////
bool CGameDownloads::Update()
{
	std::vector<Net::SFetchResult> results{};

	m_fetcher.TakeResults(results);

	for (Net::SFetchResult& result : results)
	{
		auto const pendingIcon{ m_pendingIconHashes.find(result.path) };

		// A download may ask for a pending icon's path too; the fetcher answers in order, so the first answer is the icon's.
		if (pendingIcon != m_pendingIconHashes.end())
		{
			std::string const sha256{ std::move(pendingIcon->second) };

			m_pendingIconHashes.erase(pendingIcon);
			TakeIcon(std::move(result), sha256);
		}
		else if (m_phase == EDownloadPhase::ReadingIndex)
		{
			TakeIndex(std::move(result));
		}
		else if (m_phase == EDownloadPhase::Downloading)
		{
			std::string path{ result.path };

			m_fetched.insert_or_assign(std::move(path), std::move(result));
		}
	}

	if (m_phase == EDownloadPhase::Downloading && m_fetched.size() >= m_numToFetch)
	{
		InstallFetched();
		m_phase = EDownloadPhase::Idle;
		RefreshOffers();
	}

	return std::exchange(m_hasChanged, false);
}

//////////////////////////////////////////////////////////////////////////
EDownloadPhase CGameDownloads::GetPhase() const
{
	return m_phase;
}

//////////////////////////////////////////////////////////////////////////
bool CGameDownloads::HasIndex() const
{
	return m_pIndex != nullptr;
}

//////////////////////////////////////////////////////////////////////////
std::span<SGameOffer const> CGameDownloads::GetOffers() const
{
	return m_offers;
}

//////////////////////////////////////////////////////////////////////////
std::string_view CGameDownloads::GetIcon(std::string_view sha256) const
{
	auto const it{ m_iconsByHash.find(sha256) };

	return (it != m_iconsByHash.end()) ? std::string_view{ it->second } : std::string_view{};
}

//////////////////////////////////////////////////////////////////////////
std::span<std::string const> CGameDownloads::GetDownloadKeys() const
{
	return m_downloadKeys;
}

//////////////////////////////////////////////////////////////////////////
std::span<std::string const> CGameDownloads::GetProblems() const
{
	return m_problems;
}

//////////////////////////////////////////////////////////////////////////
size_t CGameDownloads::GetNumFetched() const
{
	return m_fetched.size();
}

//////////////////////////////////////////////////////////////////////////
size_t CGameDownloads::GetNumToFetch() const
{
	return m_numToFetch;
}

//////////////////////////////////////////////////////////////////////////
void CGameDownloads::TakeIndex(Net::SFetchResult result)
{
	m_phase = EDownloadPhase::Idle;

	if (result.body.has_value())
	{
		std::expected<SGameIndex, std::string> index{ Lkt::Download::ReadIndex(*result.body) };

		if (index.has_value())
		{
			m_pIndex = std::make_unique<SGameIndex>(std::move(*index));

			if (m_indexIcons == EIndexIcons::Fetch)
			{
				RequestIcons();
			}
		}
		else
		{
			AddProblem(std::move(index.error()));
		}
	}
	else
	{
		AddProblem(std::move(result.body.error()));
	}

	RefreshOffers();
}

//////////////////////////////////////////////////////////////////////////
void CGameDownloads::RequestIcons()
{
	std::vector<Net::SFetchRequest> requests{};

	for (SIndexGame const& game : m_pIndex->games)
	{
		SIndexFile const* const pIcon{ FindIcon(game) };

		if (pIcon != nullptr && !m_iconsByHash.contains(pIcon->sha256) && !std::ranges::contains(m_pendingIconHashes | std::views::values, pIcon->sha256))
		{
			std::string path{ MakePath(m_pIndex->commit, std::format("games/{}/{}", game.key, IconFile)) };

			requests.emplace_back(path, pIcon->size);
			m_pendingIconHashes.emplace(std::move(path), pIcon->sha256);
		}
	}

	if (!requests.empty())
	{
		m_fetcher.Fetch(std::move(requests));
	}
}

//////////////////////////////////////////////////////////////////////////
// The request was capped at the indexed size, so the hash alone tells the icon is the one indexed.
void CGameDownloads::TakeIcon(Net::SFetchResult result, std::string_view sha256)
{
	if (result.body.has_value() && HashSha256(*result.body) == sha256)
	{
		m_iconsByHash.insert_or_assign(std::string{ sha256 }, std::move(*result.body));
	}
	else
	{
		gLog.Warning("Cannot show the icon {}: {}", result.path,
			result.body.has_value() ? std::string_view{ "it differs from what the index says it is" } : std::string_view{ result.body.error() });
	}
}

//////////////////////////////////////////////////////////////////////////
// Protocols first, so a game never lands without the protocol it names.
void CGameDownloads::InstallFetched()
{
	std::set<std::string> failedProtocols{};
	size_t numInstalled{ 0 };

	for (SIndexProtocol const& protocol : m_pIndex->protocols)
	{
		auto const fetched{ m_fetched.find(MakePath(m_pIndex->commit, std::format("protocols/{}", protocol.file.name))) };
		std::expected<void, std::string> installed{};

		if (fetched != m_fetched.end())
		{
			std::expected<std::string const*, std::string> const verified{ Verify(fetched->second, protocol.file) };

			installed = verified.has_value() ? InstallProtocol(m_downloadedDir, protocol.name, **verified)
				: std::expected<void, std::string>{ std::unexpected{ verified.error() } };
			m_hasChanged = m_hasChanged || installed.has_value();
		}

		if (!installed.has_value())
		{
			failedProtocols.insert(protocol.name);
			AddProblem(std::format("The protocol {}: {}", protocol.name, installed.error()));
		}
	}

	for (std::string const& key : m_downloadKeys)
	{
		SIndexGame const& game{ *FindGame(*m_pIndex, key) };
		std::map<std::string, std::string> files{};
		std::string problem{ failedProtocols.contains(game.protocol) ? std::format("not installed, as its protocol {} could not be", game.protocol)
			: std::string{} };

		for (SIndexFile const& file : game.files)
		{
			auto const fetched{ m_fetched.find(MakePath(m_pIndex->commit, std::format("games/{}/{}", key, file.name))) };
			std::expected<std::string const*, std::string> const verified{ (fetched != m_fetched.end()) ? Verify(fetched->second, file)
				: std::expected<std::string const*, std::string>{ std::unexpected{ std::format("{} was not fetched", file.name) } } };

			if (verified.has_value())
			{
				files.emplace(file.name, **verified);
			}
			else if (problem.empty())
			{
				problem = verified.error();
			}
		}

		std::expected<void, std::string> const installed{ problem.empty() ? InstallGame(m_downloadedDir, key, files)
			: std::expected<void, std::string>{ std::unexpected{ problem } } };

		if (installed.has_value())
		{
			++numInstalled;
		}
		else
		{
			AddProblem(std::format("{}: {}", game.name, installed.error()));
		}
	}

	gLog.Info("Downloaded {} of {} {} from {}", numInstalled, m_downloadKeys.size(), (m_downloadKeys.size() == 1) ? "game" : "games",
		m_source.origin.host);
	m_hasChanged = m_hasChanged || numInstalled > 0;
	m_fetched.clear();
	m_downloadKeys.clear();
}

//////////////////////////////////////////////////////////////////////////
// Before an index arrives, or when it cannot, the downloaded games still show, so they can be removed.
void CGameDownloads::RefreshOffers()
{
	std::map<std::string, std::string> downloaded{ m_downloadedDir.empty() ? std::map<std::string, std::string>{} : ListDownloadedGames(m_downloadedDir) };

	m_offers.clear();

	if (m_pIndex != nullptr)
	{
		for (SIndexGame const& game : m_pIndex->games)
		{
			SIndexProtocol const& protocol{ *FindProtocol(*m_pIndex, game.protocol) };
			EOfferState const state{ IsSupported(game, protocol) ? FindInstalledState(m_downloadedDir, game, protocol) : EOfferState::NeedsNewerLookout };

			SIndexFile const* const pIcon{ FindIcon(game) };

			m_offers.emplace_back(game.key, game.name, state, (pIcon != nullptr) ? pIcon->sha256 : std::string{}, game.protocol, protocol.version);
			downloaded.erase(game.key);
		}
	}

	for (auto const& [key, name] : downloaded)
	{
		m_offers.emplace_back(key, name, (m_pIndex != nullptr) ? EOfferState::Withdrawn : EOfferState::Installed);
	}

	std::ranges::sort(m_offers, {}, &SGameOffer::name);
}

//////////////////////////////////////////////////////////////////////////
std::string CGameDownloads::MakePath(std::string_view reference, std::string_view file) const
{
	return std::format("{}/{}/{}", m_source.repository, reference, file);
}

//////////////////////////////////////////////////////////////////////////
void CGameDownloads::AddProblem(std::string problem)
{
	gLog.Warning("{}", problem);
	m_problems.emplace_back(std::move(problem));
}
} // namespace Lkt::Download
