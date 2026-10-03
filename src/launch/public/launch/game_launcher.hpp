#pragma once

#include "launch/child_process.hpp"
#include "launch/connect_request.hpp"
#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include <tge/non_copyable.hpp>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt
{
namespace Query
{
struct SGameDefinition;
} // namespace Query

namespace Launch
{
// A game outlives Lookout: it runs in a session of its own and is never stopped.
class CGameLauncher final : private Tge::SNoCopyNoMove
{
public:

	CGameLauncher() = default;
	~CGameLauncher() = default;

	void Initialize(std::string_view logsDir);
	std::expected<void, ELaunchError> Launch(Query::SGameDefinition const& game, SLaunchOption const& option, SConnectRequest const& request);
	void ReapFinished();

private:

	std::expected<void, ELaunchError> Spawn(Query::SGameDefinition const& game, SLaunchOption const& option, SConnectRequest const& request, std::vector<std::string> const& arguments);

	std::filesystem::path m_logsDir;
	std::vector<SChildProcess> m_children;
};
} // namespace Launch
} // namespace Lkt
