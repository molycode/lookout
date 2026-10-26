#pragma once

#include "net/clock.hpp"
#include "net/fetch_result.hpp"
#include "net/https_fetcher.hpp"
#include "net/https_origin.hpp"
#include <tge/non_copyable.hpp>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Lkt::Download
{
class CLatestRelease final : private Tge::SNoCopyNoMove
{
public:

	CLatestRelease() = default;
	~CLatestRelease() = default;

	bool Initialize(Net::SHttpsOrigin origin, std::function<void()> onResults);
	void Terminate();
	void Update(Net::Clock::time_point now);

	std::optional<Net::Clock::time_point> GetNextCheck() const;
	bool IsChecking() const;
	std::string_view GetVersion() const;
	std::string_view GetProblem() const;

private:

	void TakeAnswer(Net::SFetchResult const& result, Net::Clock::time_point now);

	Net::CHttpsFetcher m_fetcher;
	std::optional<Net::Clock::time_point> m_nextCheck;
	bool m_isChecking{ false };
	std::string m_version;
	std::string m_problem;
};
} // namespace Lkt::Download
