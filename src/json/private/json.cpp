#include "json/json.hpp"
#include "loggers.hpp"
#include <cstdlib>

namespace Lkt::Json
{
//////////////////////////////////////////////////////////////////////////
void AbortOnJsonError(char const* pWhat)
{
	gLog.Error("nlohmann/json failed on a value that was not checked first: {}", pWhat);
	std::abort();
}
} // namespace Lkt::Json
