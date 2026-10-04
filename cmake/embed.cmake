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

# LktEmbedTree — compiles every file under root matching glob into the target as one
# `std::span<SEmbeddedFile const> const <symbol>` in namespace Lkt::Embedded, each file named by its path relative to
# root, in path order. The target's private headers must provide embedded_file.hpp with SEmbeddedFile.
function(LktEmbedTree target symbol root glob)
	set(generated "${CMAKE_CURRENT_BINARY_DIR}/embedded/${symbol}.cpp")

	file(GLOB_RECURSE files CONFIGURE_DEPENDS RELATIVE "${root}" "${root}/${glob}")
	list(SORT files)

	set(arrays "")
	set(entries "")
	set(index 0)

	foreach(relative IN LISTS files)
		file(READ "${root}/${relative}" hex HEX)

		if(hex STREQUAL "")
			message(FATAL_ERROR "LktEmbedTree: ${root}/${relative} is empty")
		endif()

		string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
		string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n\t" bytes "${bytes}")
		string(APPEND arrays "constexpr unsigned char Data${index}[]\n{\n\t${bytes}\n};\n\n")
		string(APPEND entries "\tSEmbeddedFile{ \"${relative}\", Data${index} },\n")
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${root}/${relative}")
		math(EXPR index "${index} + 1")
	endforeach()

	file(CONFIGURE OUTPUT "${generated}" CONTENT
"#include \"embedded_file.hpp\"
#include <array>
#include <span>

namespace Lkt::Embedded
{
namespace
{
@arrays@constexpr std::array<SEmbeddedFile, @index@> Files
{
@entries@};
} // namespace

extern std::span<SEmbeddedFile const> const ${symbol};
constinit std::span<SEmbeddedFile const> const ${symbol}{ Files };
} // namespace Lkt::Embedded
" @ONLY)

	target_sources(${target} PRIVATE "${generated}")
endfunction()
