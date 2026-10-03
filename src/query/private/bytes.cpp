#include "bytes.hpp"

namespace Lkt::Query
{
//////////////////////////////////////////////////////////////////////////
std::string_view AsText(std::span<std::byte const> bytes)
{
	return std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() };
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> ToBytes(std::string_view text)
{
	std::vector<std::byte> bytes{};

	bytes.reserve(text.size());

	for (char const c : text)
	{
		bytes.emplace_back(static_cast<std::byte>(c));
	}

	return bytes;
}
} // namespace Lkt::Query
