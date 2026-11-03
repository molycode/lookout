#include "games/named_stores.hpp"
#include "launch_layout.hpp"
#include <string_view>

namespace Lkt::Games
{
//////////////////////////////////////////////////////////////////////////
// Format 3 names Steam and GOG by its ids among the Windows facts, format 4 every store in launch.stores.
std::vector<std::string> FindNamedStores(nlohmann::ordered_json const& game)
{
	nlohmann::ordered_json copy = game;
	std::vector<std::string> stores{};

	LiftStoreIds(copy);

	nlohmann::ordered_json const& lifted{ copy };
	nlohmann::ordered_json::const_iterator const launch{ lifted.is_object() ? lifted.find("launch") : lifted.cend() };

	if (launch != lifted.cend() && launch->is_object())
	{
		nlohmann::ordered_json::const_iterator const named{ launch->find("stores") };

		if (named != launch->cend() && named->is_object())
		{
			for (auto const& item : named->items())
			{
				if (!std::string_view{ item.key() }.starts_with("//"))
				{
					stores.emplace_back(item.key());
				}
			}
		}
	}

	return stores;
}
} // namespace Lkt::Games
