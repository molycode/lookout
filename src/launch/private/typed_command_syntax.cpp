#include "typed_command_syntax.hpp"
#include <tge/platform.hpp>

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
// What the user types follows their system's own rules, so a path copied from it splits as it reads there.
ECommandSyntax GetTypedCommandSyntax()
{
#if defined(TGE_PLATFORM_WINDOWS)
	return ECommandSyntax::Windows;
#elif defined(TGE_PLATFORM_LINUX)
	return ECommandSyntax::Posix;
#else
#error "Unsupported platform"
#endif // defined(TGE_PLATFORM_WINDOWS)
}
} // namespace Lkt::Launch
