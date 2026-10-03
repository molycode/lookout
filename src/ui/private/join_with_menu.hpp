#pragma once

#include <cstddef>
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

size_t CountUsableLaunchers(Browser::CBrowser const& browser);
void DrawJoinWithItems(Browser::CBrowser const& browser, uint64_t key, SFrameIntents& intents);
// Both from the same ID scope: the popup's ID is relative to it.
void OpenJoinWithPopup();
void DrawJoinWithPopup(Browser::CBrowser const& browser, uint64_t key, SFrameIntents& intents);
void DrawJoinTooltip(Browser::CBrowser const& browser);
} // namespace Ui
} // namespace Lkt
