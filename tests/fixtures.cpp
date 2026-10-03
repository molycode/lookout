#include "fixtures.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <iterator>

namespace Lkt::Fixtures
{
namespace
{
//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> ReadFile(std::filesystem::path const& path)
{
	std::vector<std::byte> bytes{};
	std::ifstream file{ path, std::ios::binary };

	if (file.is_open())
	{
		std::vector<char> const contents{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

		bytes.reserve(contents.size());

		for (char const c : contents)
		{
			bytes.emplace_back(static_cast<std::byte>(c));
		}
	}
	else
	{
		ADD_FAILURE() << "cannot open fixture " << path;
	}

	return bytes;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
std::vector<std::byte> LoadFixture(std::string_view relativePath)
{
	return ReadFile(std::filesystem::path{ LKT_FIXTURES_DIR } / relativePath);
}

//////////////////////////////////////////////////////////////////////////
std::vector<std::filesystem::path> ListFixtures(std::string_view game, std::string_view prefix)
{
	std::vector<std::filesystem::path> paths{};
	std::error_code error{};

	for (std::filesystem::directory_iterator it{ std::filesystem::path{ LKT_FIXTURES_DIR } / game, error }, end{}; error.value() == 0 && it != end; it.increment(error))
	{
		if (it->path().filename().string().starts_with(prefix))
		{
			paths.emplace_back(it->path().lexically_relative(LKT_FIXTURES_DIR));
		}
	}

	std::ranges::sort(paths);

	if (paths.empty())
	{
		ADD_FAILURE() << "no " << prefix << " fixtures for " << game;
	}

	return paths;
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
} // namespace Lkt::Fixtures
