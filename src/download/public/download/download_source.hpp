#pragma once

#include "net/https_origin.hpp"
#include <string>

namespace Lkt::Download
{
// Where games are downloaded from: a repository laid out as lookout-games, its files under
// <repository>/<branch or commit>/ on the origin.
struct SDownloadSource final
{
	Net::SHttpsOrigin origin;
	std::string repository;
};
} // namespace Lkt::Download
