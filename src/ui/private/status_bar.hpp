#pragma once

#include <string_view>

namespace Lkt
{
namespace Browser
{
class CBrowser;
} // namespace Browser

namespace Ui
{
void DrawStatusBar(Browser::CBrowser const& browser, std::string_view message);
} // namespace Ui
} // namespace Lkt
