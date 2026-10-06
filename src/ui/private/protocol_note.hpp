#pragma once

#include <string>

namespace Lkt
{
namespace Query
{
struct SProtocolDefinition;
} // namespace Query

namespace Ui
{
std::string DescribeProtocol(Query::SProtocolDefinition const& protocol);
} // namespace Ui
} // namespace Lkt
