#include "path_list.hpp"

namespace Lkt::Launch
{
//////////////////////////////////////////////////////////////////////////
std::vector<std::filesystem::path> SplitPathList(std::string_view list)
{
	std::vector<std::filesystem::path> paths{};
	size_t start{ 0 };

	while (start <= list.size())
	{
		size_t const colon{ list.find(':', start) };
		size_t const end{ (colon != std::string_view::npos) ? colon : list.size() };
		std::filesystem::path const entry{ list.substr(start, end - start) };

		if (entry.is_absolute())
		{
			paths.emplace_back(entry);
		}

		start = end + 1;
	}

	return paths;
}
} // namespace Lkt::Launch
