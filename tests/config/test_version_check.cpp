#include "channels.hpp"
#include "config/version_check.hpp"
#include "temp_dir.hpp"
#include <tge/testing/expected_log.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace Lkt::Config
{
namespace
{
using Fixtures::ConfigChannel;
using Tge::Testing::CExpectedLog;

//////////////////////////////////////////////////////////////////////////
class CVersionCheckTest : public testing::Test
{
protected:

	// testing::Test
	void SetUp() override
	{
		ASSERT_TRUE(m_tempDir.Initialize("lookout-version-check"));
		m_dir = m_tempDir.GetPath().string();
		m_marker = m_tempDir.GetPath() / "version-check-off";
	}

	void TearDown() override
	{
		m_tempDir.Terminate();
	}
	// ~testing::Test

	void PlaceMarker() const
	{
		std::ofstream const file{ m_marker };
	}

	Fixtures::CTempDir m_tempDir{};
	std::string m_dir{};
	std::filesystem::path m_marker{};
};

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, IsOnWithoutTheMarker)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };

	EXPECT_TRUE(IsVersionCheckOn(m_dir));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, IsOffWithTheMarker)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };

	PlaceMarker();

	EXPECT_FALSE(IsVersionCheckOn(m_dir));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, IsOnInAFolderThatIsNotThere)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };

	EXPECT_TRUE(IsVersionCheckOn((m_tempDir.GetPath() / "missing").string()));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, IsOnWithoutAConfigDirectory)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };

	EXPECT_TRUE(IsVersionCheckOn({}));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, SwitchingOffWritesTheMarker)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };

	EXPECT_TRUE(SetVersionCheck(m_dir, false));
	EXPECT_TRUE(std::filesystem::exists(m_marker));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, SwitchingOnRemovesTheMarker)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };

	PlaceMarker();

	EXPECT_TRUE(SetVersionCheck(m_dir, true));
	EXPECT_FALSE(std::filesystem::exists(m_marker));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, SwitchingOnWithoutTheMarkerSucceeds)
{
	CExpectedLog const expected{ ConfigChannel, 0, 0 };

	EXPECT_TRUE(SetVersionCheck(m_dir, true));
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CVersionCheckTest, SwitchingWithoutAConfigDirectoryIsNotKept)
{
	CExpectedLog const expected{ ConfigChannel, 1, 0 };

	EXPECT_FALSE(SetVersionCheck({}, false));
}
} // namespace
} // namespace Lkt::Config
