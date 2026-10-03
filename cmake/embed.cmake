# LktEmbedFile — compiles a file's bytes into the target as `std::span<unsigned char const> const <symbol>`
# in namespace Lkt::Embedded, so the shipped binary needs no data files beside it.
function(LktEmbedFile target symbol file)
	set(generated "${CMAKE_CURRENT_BINARY_DIR}/embedded/${symbol}.cpp")

	file(READ "${file}" hex HEX)
	string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
	string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n\t" bytes "${bytes}")

	file(CONFIGURE OUTPUT "${generated}" CONTENT
"#include <span>

namespace Lkt::Embedded
{
namespace
{
unsigned char const Data[]
{
	${bytes}
};
} // namespace

extern std::span<unsigned char const> const ${symbol};
std::span<unsigned char const> const ${symbol}{ Data };
} // namespace Lkt::Embedded
" @ONLY)

	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
	target_sources(${target} PRIVATE "${generated}")
endfunction()
