#pragma once

#include <imgui.h>

namespace Lkt::Ui
{
// The theme's colours the views draw with directly.
struct SThemeColors final
{
	ImVec4 text;
	ImVec4 textDisabled;
	ImVec4 amber;
	ImVec4 pingGood;
	ImVec4 pingFair;
	ImVec4 pingPoor;
	ImVec4 error;
};
} // namespace Lkt::Ui
