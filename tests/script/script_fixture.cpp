#include "script_fixture.hpp"
#include <format>

namespace Lkt::Fixtures
{
//////////////////////////////////////////////////////////////////////////
std::string MakeScript(std::string_view body)
{
	return std::format(R"lua(local protocol = {{
	api = 1,
	masterRequest = function(options) return "master" end,
	statusRequest = function(options) return "status" end,
	parseMasterReply = function(datagram) return {{}} end,
	parseStatusReply = function(datagram) return {{ rules = {{}}, players = {{}}, malformedPlayerLines = 0 }} end,
}}
{}
return protocol
)lua", body);
}

//////////////////////////////////////////////////////////////////////////
void CProtocolScriptTest::TearDown()
{
	m_script.Terminate();
}

//////////////////////////////////////////////////////////////////////////
std::expected<void, std::string> CProtocolScriptTest::Load(std::string_view body)
{
	return m_script.Initialize("test", MakeScript(body));
}
} // namespace Lkt::Fixtures
