#include "run_context.hpp"
#include <tge/module/core_module.hpp>
#include <cstddef>

namespace Lkt
{
namespace
{
using namespace Tge;

IModule* const Modules[]
{
	Core::gModule
};

SScheduleEntry const Schedule[]
{
	{ EFramePhase::FrameStart, Core::gModule }
};

// Lookout queues no jobs: its waiting runs on the query engine's event loop.
constexpr size_t NumWorkerThreads{ 0 };
} // namespace

//////////////////////////////////////////////////////////////////////////
Tge::SRunContext MakeRunContext(std::string_view logsDir, std::string_view configDir)
{
	return Tge::SRunContext{ Modules, Schedule, {}, logsDir, configDir, NumWorkerThreads };
}
} // namespace Lkt
