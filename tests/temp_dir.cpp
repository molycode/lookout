#include "temp_dir.hpp"
#include <format>
#include <random>
#include <system_error>

namespace Lkt::Fixtures
{
namespace
{
constexpr int MaxAttempts{ 100 };
} // namespace

//////////////////////////////////////////////////////////////////////////
// A new, empty folder of its own under the system's temporary folder, so tests never share one.
bool CTempDir::Initialize(std::string_view prefix)
{
	std::error_code error{};
	std::filesystem::path const base{ std::filesystem::temp_directory_path(error) };
	std::random_device random{};

	for (int attempt{ 0 }; attempt < MaxAttempts && m_path.empty() && error.value() == 0; ++attempt)
	{
		std::filesystem::path const candidate{ base / std::format("{}-{:08x}", prefix, random()) };

		if (std::filesystem::create_directory(candidate, error))
		{
			m_path = candidate;
		}
	}

	return !m_path.empty();
}

//////////////////////////////////////////////////////////////////////////
void CTempDir::Terminate()
{
	std::error_code error{};

	if (!m_path.empty())
	{
		std::filesystem::remove_all(m_path, error);
		m_path.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
std::filesystem::path const& CTempDir::GetPath() const
{
	return m_path;
}
} // namespace Lkt::Fixtures
