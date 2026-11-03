#include "channels.hpp"
#include "store_source.hpp"
#include "temp_dir.hpp"
#include "launch/found_store.hpp"
#include "query/store_id.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Launch
{
namespace
{
using Fixtures::LaunchChannel;
using Tge::Testing::CExpectedLog;

//////////////////////////////////////////////////////////////////////////
void WriteManifest(std::filesystem::path const& library, std::string_view stateFlags)
{
	std::filesystem::create_directories(library / "steamapps");

	std::ofstream file{ library / "steamapps" / "appmanifest_38430.acf", std::ios::binary };

	file << std::format("\"AppState\"\n{{\n\t\"name\"\t\t\"Kingpin\"\n\t\"StateFlags\"\t\t\"{}\"\n\t\"installdir\"\t\t\"Kingpin\"\n}}\n", stateFlags);
}

//////////////////////////////////////////////////////////////////////////
class CStoreSourceTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-store-source"));
		m_library = m_tempDir.GetPath() / "SteamLibrary";
		m_stores = { SFoundStore{ "steam", "Steam", { "-applaunch", "{id}" }, Query::EStoreLibraries::Steam, m_tempDir.GetPath() / "steam", { m_library } } };
		m_hints.stores = { Query::SStoreId{ "steam", "38430" } };
		WriteManifest(m_library, "4");
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	std::vector<SLaunchOption> Find() const
	{
		CStoreSource source{};

		source.Initialize(m_stores);

		std::vector<SLaunchOption> options{ source.Find(m_hints, "Kingpin: Life of Crime") };

		source.Terminate();

		return options;
	}

	Lkt::Fixtures::CTempDir m_tempDir{};
	std::filesystem::path m_library{};
	std::vector<SFoundStore> m_stores{};
	Query::SLaunchHints m_hints{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CStoreSourceTest, GameInstalledInTheStoreStartsThroughIt)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };
	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options.front(), (SLaunchOption{ "store:steam", "Kingpin", "through Steam", { m_stores.front().program.string(), "-applaunch", "38430" }, {} }));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStoreSourceTest, GameTheStoreHasNotFinishedInstallingIsNotOffered)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	WriteManifest(m_library, "2");

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStoreSourceTest, GameWithoutAnIdInTheStoreIsNotOffered)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	m_hints.stores.clear();

	EXPECT_TRUE(Find().empty());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStoreSourceTest, StoreFoundTwiceOffersTheGameOnce)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	m_stores.emplace_back(m_stores.front());

	EXPECT_EQ(Find().size(), 1u);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CStoreSourceTest, EveryIdPlaceholderIsFilled)
{
	CExpectedLog const expected{ LaunchChannel, 0, 0 };

	m_stores.front().arguments = { "steam://run/{id}//{id}" };

	std::vector<SLaunchOption> const options{ Find() };

	ASSERT_EQ(options.size(), 1u);
	EXPECT_EQ(options.front().argv.back(), "steam://run/38430//38430");
}
} // namespace
} // namespace Lkt::Launch
