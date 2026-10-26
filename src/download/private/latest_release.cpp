#include "download/latest_release.hpp"
#include "loggers.hpp"
#include "read_release.hpp"
#include "download/lookout_release.hpp"
#include "net/fetch_request.hpp"
#include <chrono>
#include <cstddef>
#include <expected>
#include <utility>
#include <vector>

namespace Lkt::Download
{
namespace
{
constexpr std::chrono::hours CheckInterval{ 24 };
// Soon enough after a start without network, and far below GitHub's 60 unauthenticated requests an hour.
constexpr std::chrono::hours RetryInterval{ 1 };
// The reply carries the release notes and the assets, so it grows with them.
constexpr size_t MaxReleaseSize{ 256 * 1024 };
} // namespace

//////////////////////////////////////////////////////////////////////////
bool CLatestRelease::Initialize(Net::SHttpsOrigin origin, std::function<void()> onResults)
{
	bool const isReady{ m_fetcher.Initialize(std::move(origin), std::move(onResults)) };

	m_nextCheck = isReady ? std::optional<Net::Clock::time_point>{ Net::Clock::time_point{} } : std::nullopt;

	return isReady;
}

//////////////////////////////////////////////////////////////////////////
void CLatestRelease::Terminate()
{
	m_fetcher.Terminate();
	m_nextCheck.reset();
	m_isChecking = false;
	m_version.clear();
	m_problem.clear();
}

//////////////////////////////////////////////////////////////////////////
// The next check is a day off from the moment one starts, so a deadline already passed never comes back.
void CLatestRelease::Update(Net::Clock::time_point now)
{
	if (m_nextCheck.has_value())
	{
		std::vector<Net::SFetchResult> results{};

		m_fetcher.TakeResults(results);

		for (Net::SFetchResult const& result : results)
		{
			TakeAnswer(result, now);
		}

		if (!m_isChecking && now >= *m_nextCheck)
		{
			m_isChecking = true;
			m_nextCheck = now + CheckInterval;
			m_fetcher.Fetch({ Net::SFetchRequest{ std::string{ LookoutLatestReleasePath }, MaxReleaseSize } });
		}
	}
}

//////////////////////////////////////////////////////////////////////////
std::optional<Net::Clock::time_point> CLatestRelease::GetNextCheck() const
{
	return m_nextCheck;
}

//////////////////////////////////////////////////////////////////////////
bool CLatestRelease::IsChecking() const
{
	return m_isChecking;
}

//////////////////////////////////////////////////////////////////////////
std::string_view CLatestRelease::GetVersion() const
{
	return m_version;
}

//////////////////////////////////////////////////////////////////////////
std::string_view CLatestRelease::GetProblem() const
{
	return m_problem;
}

//////////////////////////////////////////////////////////////////////////
// A failure keeps the version an earlier check found, and is logged only when it differs from the last one.
void CLatestRelease::TakeAnswer(Net::SFetchResult const& result, Net::Clock::time_point now)
{
	std::expected<std::string, std::string> version{ result.body.has_value() ? ReadReleaseVersion(*result.body) : std::unexpected{ result.body.error() } };

	m_isChecking = false;

	if (version.has_value())
	{
		m_version = std::move(*version);
		m_problem.clear();
	}
	else
	{
		if (version.error() != m_problem)
		{
			gLog.Warning("Cannot tell whether a newer Lookout is out: {}", version.error());
		}

		m_problem = std::move(version.error());
		m_nextCheck = now + RetryInterval;
	}
}
} // namespace Lkt::Download
