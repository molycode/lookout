#include "tge_environment.hpp"
#include <tge/init/init.hpp>
#include <tge/logging/log_level.hpp>
#include <tge/logging/log_system.hpp>

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
void CTgeEnvironment::SetUp()
{
	Tge::Logging::CLogSystem& logSystem{ Tge::Logging::GetLogSystem() };

	// Running, so a CExpectedLog's listener sees messages; terminal only, so nothing else queues for listeners.
	logSystem.Initialize("test", "", "");
	logSystem.SetEnabledTargets(Tge::Logging::ETarget::Terminal);

	// No workers, as the app runs: nothing in Lookout queues jobs.
	Tge::Initialize(0);
}

//////////////////////////////////////////////////////////////////////////
void CTgeEnvironment::TearDown()
{
	Tge::Terminate();
	Tge::Logging::GetLogSystem().Terminate();
}
} // namespace Lkt::Fixtures
