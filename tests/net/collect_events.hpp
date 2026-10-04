#pragma once

#include "net/query_event.hpp"
#include <span>
#include <variant>
#include <vector>

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
template<typename TEvent>
std::vector<TEvent> CollectEvents(std::span<Net::SQueryEvent const> events)
{
	std::vector<TEvent> found{};

	for (Net::SQueryEvent const& event : events)
	{
		if (TEvent const* const pTyped{ std::get_if<TEvent>(&event) }; pTyped != nullptr)
		{
			found.emplace_back(*pTyped);
		}
	}

	return found;
}
} // namespace Lkt::Fixtures
