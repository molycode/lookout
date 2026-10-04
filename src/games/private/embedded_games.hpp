#pragma once

#include "embedded_file.hpp"
#include <span>

namespace Lkt::Embedded
{
extern std::span<SEmbeddedFile const> const Protocols;
extern std::span<SEmbeddedFile const> const Games;
extern std::span<SEmbeddedFile const> const GameIcons;
extern std::span<unsigned char const> const NewGame;
} // namespace Lkt::Embedded
