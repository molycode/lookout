#pragma once

#include "launch/launch_error.hpp"
#include "launch/launch_option.hpp"
#include "query/server_address.hpp"
#include <expected>
#include <string>
#include <string_view>

namespace Lkt::Ui
{
std::string DescribeJoin(std::string_view gameName, Query::SServerAddress const& address, std::expected<Launch::SLaunchOption, Launch::ELaunchError> const& launcher,
	std::expected<void, Launch::ELaunchError> const& result);
} // namespace Lkt::Ui
