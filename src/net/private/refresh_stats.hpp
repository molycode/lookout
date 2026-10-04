#pragma once

#include "query/parse_error.hpp"
#include "query/server_address.hpp"
#include <cstdint>
#include <string>

namespace Lkt::Net
{
struct SRefreshStats final
{
	size_t numListed{ 0 };
	size_t numAnswered{ 0 };
	size_t numNoAnswer{ 0 };
	Query::SServerAddress firstNoAnswer;
	size_t numBadReplies{ 0 };
	Query::SServerAddress firstBadReply;
	Query::EParseError firstBadReplyError{ Query::EParseError::Malformed };
	size_t numScriptFailures{ 0 };
	std::string firstScriptFailure;
	size_t numMalformedPlayerLines{ 0 };
	size_t numUnqueryable{ 0 };
	Query::SServerAddress firstUnqueryable;
	size_t numBadMasterDatagrams{ 0 };
	Query::EParseError firstBadMasterDatagramError{ Query::EParseError::Malformed };
	size_t numSendFailures{ 0 };
	Query::SServerAddress firstSendFailure;
	int firstSendError{ 0 };
	size_t numReceiveErrors{ 0 };
	int firstReceiveError{ 0 };
	size_t numOverCap{ 0 };
	Query::SServerAddress firstOverCap;
};
} // namespace Lkt::Net
