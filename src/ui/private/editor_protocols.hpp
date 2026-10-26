#pragma once

#include "form_protocols.hpp"
#include "query/protocol_definition.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt
{
namespace Download
{
class CGameDownloads;
} // namespace Download

namespace Games
{
struct SGameFormNode;
} // namespace Games

namespace Ui
{
class CEditorProtocols final : private Tge::SNoCopyNoMove
{
public:

	CEditorProtocols() = default;
	~CEditorProtocols() = default;

	void Refresh();
	bool Update(Download::CGameDownloads* pDownloads, std::filesystem::path const& userDir, Games::SGameFormNode const& form);
	std::span<Query::SProtocolDefinition const> GetDefinitions() const;
	SFormProtocols MakeFormProtocols(Download::CGameDownloads* pDownloads);

private:

	bool DrawMoreChoices(std::string& protocol, Download::CGameDownloads* pDownloads);
	void DrawAbsent(std::string_view protocol, Download::CGameDownloads const* pDownloads);
	std::string FindWanted(Download::CGameDownloads const& downloads, Games::SGameFormNode const& form) const;
	bool TakeFetched(Download::CGameDownloads const& downloads, std::filesystem::path const& userDir);
	bool IsDefined(std::string_view name) const;

	std::vector<Query::SProtocolDefinition> m_definitions;
	std::vector<Query::SProtocolDefinition> m_fetched;
	std::map<std::string, std::string, std::less<>> m_failures;
	std::string m_pending;
	bool m_isIndexAsked{ false };
};
} // namespace Ui
} // namespace Lkt
