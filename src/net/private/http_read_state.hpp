#pragma once

#include <cstdint>

namespace Lkt::Net
{
enum class EHttpReadState : uint8_t
{
	Headers,
	Body,
	Complete,
	Failed
};
} // namespace Lkt::Net
