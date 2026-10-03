#pragma once

#include <cstdint>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Ui
{
struct SFrameIntents;

void DrawServerTable(Browser::CBrowser const& browser, uint64_t& selectedKey, bool& shouldScrollToSelection, SFrameIntents& intents);
} // namespace Ui
} // namespace Lkt
