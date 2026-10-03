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

void DrawServerDetails(Browser::CBrowser const& browser, uint64_t selectedKey, SFrameIntents& intents);
} // namespace Ui
} // namespace Lkt
