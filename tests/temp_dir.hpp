#pragma once

#include <tge/non_copyable.hpp>
#include <filesystem>
#include <string_view>

namespace Lkt::Fixtures
{
class CTempDir final : private Tge::SNoCopyNoMove
{
public:

	CTempDir() = default;
	~CTempDir() = default;

	bool Initialize(std::string_view prefix);
	void Terminate();

	std::filesystem::path const& GetPath() const;

private:

	std::filesystem::path m_path{};
};
} // namespace Lkt::Fixtures
