#pragma once

#include <string>

namespace Lkt::Games
{
struct SFieldProblem;
struct SGameFormNode;
} // namespace Lkt::Games

namespace Lkt::Ui
{
bool DrawGameForm(Games::SGameFormNode& form, Games::SGameFormNode const* pDownloaded, Games::SFieldProblem const* pProblem);
std::string DescribeFieldProblem(Games::SFieldProblem const& problem);
} // namespace Lkt::Ui
