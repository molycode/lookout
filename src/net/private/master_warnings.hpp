#pragma once

#include "master_id.hpp"
#include "query/game.hpp"
#include <tge/non_copyable.hpp>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace Lkt::Net
{
class CMasterWarnings final : private Tge::SNoCopyNoMove
{
public:

	CMasterWarnings() = default;
	~CMasterWarnings() = default;

	bool RecordFailure(SMasterId const& master, std::string_view failure);
	void RecordAnswer(SMasterId const& master);

private:

	std::map<std::pair<Query::EGame, size_t>, std::string> m_failures;
};
} // namespace Lkt::Net
