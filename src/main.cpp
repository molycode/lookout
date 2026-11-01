#include "lookout.hpp"
#include "cli/session.hpp"
#include <span>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	Lkt::Cli::CSession session{};
	Lkt::CLookout lookout{};

	session.Initialize();

	int const result{ session.Run(std::span<char* const>{ argv, static_cast<size_t>(argc) }, [&lookout](Lkt::Cli::CSession& running)
	{
		return lookout.Run(running);
	}) };

	session.Terminate();

	return result;
}
