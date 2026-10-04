#pragma once

#include "config/settings.hpp"
#include <tge/non_copyable.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Lkt::Config
{
// Every problem is logged here, so callers only pass settings in and out.
class CSettingsStore final : private Tge::SNoCopyNoMove
{
public:

	CSettingsStore() = default;
	~CSettingsStore() = default;

	void Initialize(std::string_view configDir);
	SSettings Load();
	void Save(SSettings const& settings);
	std::string Snapshot(SSettings const& settings) const;
	SSettings Restore(std::string_view snapshot);

private:

	void KeepBackup(std::string_view problem);
	void MoveAside(std::string_view reason);

	std::filesystem::path m_path;
	std::string m_kept;
	std::optional<SSettings> m_written;
	bool m_canSave{ false };
};
} // namespace Lkt::Config
