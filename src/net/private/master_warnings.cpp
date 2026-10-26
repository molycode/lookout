#include "master_warnings.hpp"

namespace Lkt::Net
{
//////////////////////////////////////////////////////////////////////////
// A dead master fails the same way on every refresh, auto refresh included; only a new failure is worth a warning.
bool CMasterWarnings::RecordFailure(SMasterId const& master, std::string_view failure)
{
	auto const [it, isNew]{ m_failures.try_emplace(std::pair{ master.game, master.index }, failure) };
	bool const isWarned{ isNew || it->second != failure };

	it->second = failure;

	return isWarned;
}

//////////////////////////////////////////////////////////////////////////
void CMasterWarnings::RecordAnswer(SMasterId const& master)
{
	m_failures.erase(std::pair{ master.game, master.index });
}
} // namespace Lkt::Net
