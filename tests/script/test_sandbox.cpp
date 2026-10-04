#include "script_fixture.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Fixtures
{
namespace
{
std::map<std::string, std::string> const NoOptions{};

//////////////////////////////////////////////////////////////////////////
// The names a table holds, sorted; the script lists them, since the sandbox has no table.sort.
std::vector<std::string> ListNames(Script::CProtocolScript& script, std::string_view table)
{
	std::vector<std::string> names{};
	std::expected<void, std::string> const loaded{ script.Initialize("test", MakeScript(std::format(R"lua(
		protocol.masterRequest = function()
			local names = {{}}
			for name in pairs({}) do names[#names + 1] = name end
			return table.concat(names, ",")
		end)lua", table))) };

	EXPECT_TRUE(loaded.has_value()) << loaded.error_or("");

	std::expected<std::vector<std::byte>, std::string> const listed{ script.MasterRequest(NoOptions) };
	std::string const text{ listed.has_value() ? std::string{ reinterpret_cast<char const*>(listed->data()), listed->size() } : std::string{} };

	for (size_t start{ 0 }; start < text.size();)
	{
		size_t const end{ std::min(text.find(',', start), text.size()) };

		names.emplace_back(text.substr(start, end - start));
		start = end + 1;
	}

	std::ranges::sort(names);

	return names;
}

//////////////////////////////////////////////////////////////////////////
std::string ToText(std::expected<std::vector<std::byte>, std::string> const& request)
{
	return request.has_value() ? std::string{ reinterpret_cast<char const*>(request->data()), request->size() } : request.error();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, GlobalsAreOnlyTheAllowedOnes)
{
	std::vector<std::string> const expected{ "assert", "error", "ipairs", "math", "next", "pairs", "select", "string", "table", "tonumber",
		"tostring", "type", "utf8" };

	EXPECT_EQ(ListNames(m_script, "_ENV"), expected);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StringLibraryHasNoPatternFunctions)
{
	std::vector<std::string> const expected{ "byte", "char", "find", "format", "len", "lower", "pack", "packsize", "rep", "reverse", "sub",
		"unpack", "upper" };

	EXPECT_EQ(ListNames(m_script, "string"), expected);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, TableLibraryHasNoSort)
{
	std::vector<std::string> const expected{ "concat", "insert", "move", "remove", "unpack" };

	EXPECT_EQ(ListNames(m_script, "table"), expected);
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, StringMethodsReachOnlyTheSandbox)
{
	EXPECT_TRUE(Load("assert(('x').match == nil and ('x').gsub == nil and ('x').gmatch == nil and ('x').dump == nil)").has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, FindIgnoresPatternCharacters)
{
	EXPECT_TRUE(Load("assert(string.find('a.b', '.') == 2 and ('a%b'):find('%', 1, false) == 2)").has_value());
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, BinaryChunkIsRefused)
{
	std::expected<void, std::string> const loaded{ m_script.Initialize("test", "\x1bLua\x55") };

	ASSERT_FALSE(loaded.has_value());
	EXPECT_TRUE(loaded.error().contains("binary")) << loaded.error();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, EndlessLoopRunsOutOfTime)
{
	ASSERT_TRUE(Load("protocol.masterRequest = function() while true do end end").has_value());

	std::chrono::steady_clock::time_point const start{ std::chrono::steady_clock::now() };
	std::expected<std::vector<std::byte>, std::string> const request{ m_script.MasterRequest(NoOptions) };

	ASSERT_FALSE(request.has_value());
	EXPECT_TRUE(request.error().contains("test:8: the script ran out of time")) << request.error();
	EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds{ 1 });
}

//////////////////////////////////////////////////////////////////////////
// Each copy runs in C between instruction checks; the allocator stops it.
TEST_F(CProtocolScriptTest, LoopOfLargeCopiesRunsOutOfTime)
{
	ASSERT_TRUE(Load("protocol.masterRequest = function() local big = ('x'):rep(4000000) while true do local copy = big .. 'y' end end").has_value());

	std::chrono::steady_clock::time_point const start{ std::chrono::steady_clock::now() };
	std::expected<std::vector<std::byte>, std::string> const request{ m_script.MasterRequest(NoOptions) };

	ASSERT_FALSE(request.has_value());
	EXPECT_TRUE(request.error().contains("ran out of time")) << request.error();
	EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds{ 1 });
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, MemoryBlowupFails)
{
	ASSERT_TRUE(Load("protocol.masterRequest = function() return ('x'):rep(1 << 30) end").has_value());

	std::expected<std::vector<std::byte>, std::string> const request{ m_script.MasterRequest(NoOptions) };

	ASSERT_FALSE(request.has_value());
	EXPECT_TRUE(request.error().contains("not enough memory")) << request.error();
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ScriptStaysUsableAfterRunningOutOfTime)
{
	ASSERT_TRUE(Load("protocol.masterRequest = function() while true do end end").has_value());
	ASSERT_FALSE(m_script.MasterRequest(NoOptions).has_value());

	EXPECT_EQ(ToText(m_script.StatusRequest(NoOptions)), "status");
}

//////////////////////////////////////////////////////////////////////////
TEST_F(CProtocolScriptTest, ScriptStaysUsableAfterRunningOutOfMemory)
{
	ASSERT_TRUE(Load("protocol.masterRequest = function() return ('x'):rep(1 << 30) end").has_value());
	ASSERT_FALSE(m_script.MasterRequest(NoOptions).has_value());

	EXPECT_EQ(ToText(m_script.StatusRequest(NoOptions)), "status");
}
} // namespace
} // namespace Lkt::Fixtures
