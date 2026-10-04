#pragma once

#include <filesystem>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Ui
{
struct SFrameIntents;

// The data folder is where game descriptions are edited; empty, they cannot be.
void DrawGameSidebar(Browser::CBrowser const& browser, std::filesystem::path const& userDir, SFrameIntents& intents);
} // namespace Ui
} // namespace Lkt
