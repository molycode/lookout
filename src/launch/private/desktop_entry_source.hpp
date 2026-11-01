#pragma once

#include "launch/launch_environment.hpp"
#include "launch/launch_option.hpp"
#include "query/launch_hints.hpp"
#include <tge/non_copyable.hpp>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
class CDesktopEntrySource final : private Tge::SNoCopyNoMove
{
public:

	CDesktopEntrySource() = default;
	~CDesktopEntrySource() = default;

	void Initialize(SLaunchEnvironment const& environment);
	void Terminate();

	std::vector<SLaunchOption> Find(Query::SLaunchHints const& hints) const;

private:

	SLaunchEnvironment const* m_pEnvironment{ nullptr };
};
} // namespace Lkt::Launch
