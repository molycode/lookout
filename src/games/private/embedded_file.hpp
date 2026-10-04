#pragma once

#include <span>
#include <string_view>

namespace Lkt::Embedded
{
// A file LktEmbedTree compiled in, named by its path under the embedded folder.
struct SEmbeddedFile final
{
	std::string_view name;
	std::span<unsigned char const> bytes;
};
} // namespace Lkt::Embedded
