#include "layer_name.hpp"

namespace Lkt::Games
{
//////////////////////////////////////////////////////////////////////////
std::string NameLayer(std::filesystem::path const& folder)
{
	std::filesystem::path const name{ folder.has_filename() ? folder.filename() : folder.parent_path().filename() };

	return name.string();
}
} // namespace Lkt::Games
