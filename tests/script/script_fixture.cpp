#include "script_fixture.hpp"
#include "conversation_driver.hpp"
#include "fixtures.hpp"
#include <format>
#include <map>

namespace Lkt::Fixtures
{
namespace
{
std::map<std::string, std::string> const NoOptions{};
} // namespace

//////////////////////////////////////////////////////////////////////////
std::string MakeScript(std::string_view body)
{
	return std::format(R"lua(local protocol = {{
	api = 1,
	master = {{
		transport = "udp",
		start = function(options, state) return {{ send = {{ "master" }} }} end,
		receive = function(state, data) return nil end,
	}},
	server = {{
		start = function(options, state) return {{ send = {{ "status" }} }} end,
		receive = function(state, datagram) return {{ reply = {{ rules = {{}}, players = {{}} }} }} end,
	}},
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

//////////////////////////////////////////////////////////////////////////
std::expected<Script::SScriptAction, std::string> CProtocolScriptTest::Start(Script::EConversationKind kind)
{
	return StartOnce(m_script, kind, NoOptions);
}

//////////////////////////////////////////////////////////////////////////
std::expected<Script::SScriptAction, std::string> CProtocolScriptTest::Receive(Script::EConversationKind kind, std::string_view data)
{
	return ReceiveOnce(m_script, kind, NoOptions, ToBytes(data));
}
} // namespace Lkt::Fixtures
