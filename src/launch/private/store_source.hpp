#pragma once

#include "launch/found_store.hpp"
#include "launch/launch_option.hpp"
#include "query/launch_hints.hpp"
#include <tge/non_copyable.hpp>
#include <span>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
class CStoreSource final : private Tge::SNoCopyNoMove
{
public:

	CStoreSource() = default;
	~CStoreSource() = default;

	void Initialize(std::span<SFoundStore const> stores);
	void Terminate();

	std::vector<SLaunchOption> Find(Query::SLaunchHints const& hints, std::string_view gameName) const;

private:

	std::span<SFoundStore const> m_stores{};
};
} // namespace Lkt::Launch
