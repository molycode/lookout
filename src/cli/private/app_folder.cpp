#include "app_folder.hpp"
#include <tge/io/get_user_folder.hpp>
#include <tge/platform.hpp>
#include <string_view>

namespace Lkt::Cli
{
namespace
{
#if defined(TGE_PLATFORM_WINDOWS)
constexpr std::string_view AppFolderName{ "Lookout" };
#elif defined(TGE_PLATFORM_LINUX)
constexpr std::string_view AppFolderName{ "lookout" };
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
} // namespace

//////////////////////////////////////////////////////////////////////////
std::expected<std::filesystem::path, std::error_code> GetAppFolder(Tge::IO::EUserFolder folder)
{
	return Tge::IO::GetUserFolder(folder).transform([](std::filesystem::path const& base) { return base / AppFolderName; });
}
} // namespace Lkt::Cli
