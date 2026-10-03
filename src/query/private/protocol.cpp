#include "query/protocol.hpp"
#include "quake2_protocol.hpp"
#include "quake3_protocol.hpp"
#include <tge/assert.hpp>

namespace Lkt::Query
{
namespace
{
CQuake2Protocol const gQuake2Protocol{};
CQuake3Protocol const gQuake3Protocol{};
} // namespace

//////////////////////////////////////////////////////////////////////////
IProtocol const& GetProtocol(EProtocolFamily family)
{
	IProtocol const* pProtocol{ nullptr };

	switch (family)
	{
		case EProtocolFamily::Quake2:
			pProtocol = &gQuake2Protocol;
			break;

		case EProtocolFamily::Quake3:
			pProtocol = &gQuake3Protocol;
			break;
	}

	TGE_ASSERT(pProtocol != nullptr, "EProtocolFamily value without a protocol");

	return *pProtocol;
}
} // namespace Lkt::Query
